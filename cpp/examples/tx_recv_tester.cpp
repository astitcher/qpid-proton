/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include "options.hpp"

#include <proton/connection.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/message_id.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/source.hpp>
#include <proton/tracker.hpp>
#include <proton/types.hpp>

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

enum class DispositionType {
    ACCEPT,
    REJECT,
    RELEASE,
    MODIFY
};

enum class TransactionAction {
    COMMIT,
    ABORT
};

struct CallbackLog {
    std::string callback_name;
    std::string details;
    std::chrono::steady_clock::time_point timestamp;
    int sequence;
    
    CallbackLog(const std::string& name, const std::string& det = "")
        : callback_name(name), details(det), timestamp(std::chrono::steady_clock::now()), sequence(0) {}
};

class tx_recv_tester : public proton::messaging_handler {
  private:
    proton::receiver receiver;
    std::string conn_url_;
    std::string addr_;
    int total_messages_;
    int batch_size_;
    bool settle_before_discharge_;
    
    // Message tracking
    int received_count_ = 0;
    int current_batch_ = 0;
    int batch_index_ = 0;
    std::vector<proton::delivery> current_deliveries_;
    std::map<proton::binary, proton::tracker> delivery_trackers_;
    
    // Disposition configuration
    std::vector<DispositionType> disposition_sequence_;
    int disposition_index_ = 0;
    
    // Transaction action configuration
    std::vector<TransactionAction> transaction_actions_;
    int transaction_action_index_ = 0;
    
    // Callback tracking
    std::vector<CallbackLog> callback_log_;
    int callback_sequence_ = 0;
    
    // Callback counts
    int count_transaction_declared_ = 0;
    int count_transaction_committed_ = 0;
    int count_transaction_aborted_ = 0;
    int count_transaction_error_ = 0;
    int count_transactional_accept_ = 0;
    int count_transactional_reject_ = 0;
    int count_transactional_release_ = 0;
    int count_tracker_settle_ = 0;
    int count_delivery_settle_ = 0;
    
    // State verification
    proton::binary current_transaction_id_;
    bool in_transaction_ = false;
    bool discharging_ = false;
    
    // Test results
    bool test_passed_ = true;
    std::vector<std::string> test_failures_;
    
    // Timing
    std::chrono::steady_clock::time_point start_time_;
    
    void log_callback(const std::string& name, const std::string& details = "") {
        CallbackLog log(name, details);
        log.sequence = ++callback_sequence_;
        callback_log_.push_back(log);
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count();
        
        std::cout << "[" << std::setw(6) << log.sequence << "] [" << std::setw(10) << elapsed_ms << "ms] "
                  << name;
        if (!details.empty()) {
            std::cout << " - " << details;
        }
        std::cout << std::endl;
    }
    
    void verify_state(const std::string& context, bool condition, const std::string& message) {
        if (!condition) {
            test_passed_ = false;
            std::string failure = context + ": " + message;
            test_failures_.push_back(failure);
            std::cout << "  ❌ FAIL: " << failure << std::endl;
        } else {
            std::cout << "  ✅ PASS: " << message << std::endl;
        }
    }
    
    DispositionType get_next_disposition() {
        if (disposition_sequence_.empty()) {
            return DispositionType::ACCEPT; // Default
        }
        DispositionType result = disposition_sequence_[disposition_index_ % disposition_sequence_.size()];
        disposition_index_++;
        return result;
    }
    
    TransactionAction get_next_transaction_action() {
        if (transaction_actions_.empty()) {
            return TransactionAction::COMMIT; // Default
        }
        TransactionAction result = transaction_actions_[transaction_action_index_ % transaction_actions_.size()];
        transaction_action_index_++;
        return result;
    }
    
    void apply_disposition(proton::delivery& d, DispositionType type) {
        switch (type) {
            case DispositionType::ACCEPT:
                d.accept();
                log_callback("delivery.accept()", "Applied ACCEPT disposition");
                break;
            case DispositionType::REJECT:
                d.reject();
                log_callback("delivery.reject()", "Applied REJECT disposition");
                break;
            case DispositionType::RELEASE:
                d.release();
                log_callback("delivery.release()", "Applied RELEASE disposition");
                break;
            case DispositionType::MODIFY:
                d.modify();
                log_callback("delivery.modify()", "Applied MODIFY disposition");
                break;
        }
    }

  public:
    tx_recv_tester(const std::string& u, const std::string& a, int total, int batch,
                   bool settle_before_discharge = false)
        : conn_url_(u), addr_(a), total_messages_(total), batch_size_(batch),
          settle_before_discharge_(settle_before_discharge) {}
    
    void set_disposition_sequence(const std::vector<DispositionType>& seq) {
        disposition_sequence_ = seq;
    }
    
    void set_transaction_actions(const std::vector<TransactionAction>& actions) {
        transaction_actions_ = actions;
    }
    
    void print_summary() {
        std::cout << "\n=== TEST SUMMARY ===" << std::endl;
        std::cout << "Callbacks:" << std::endl;
        std::cout << "  on_session_transaction_declared: " << count_transaction_declared_ << std::endl;
        std::cout << "  on_transactional_accept: " << count_transactional_accept_ << std::endl;
        std::cout << "  on_transactional_reject: " << count_transactional_reject_ << std::endl;
        std::cout << "  on_transactional_release: " << count_transactional_release_ << std::endl;
        std::cout << "  on_session_transaction_committed: " << count_transaction_committed_ << std::endl;
        std::cout << "  on_session_transaction_aborted: " << count_transaction_aborted_ << std::endl;
        std::cout << "  on_session_transaction_error: " << count_transaction_error_ << std::endl;
        std::cout << "  on_tracker_settle: " << count_tracker_settle_ << std::endl;
        std::cout << "  on_delivery_settle: " << count_delivery_settle_ << std::endl;
        
        if (!test_failures_.empty()) {
            std::cout << "\n❌ TEST FAILURES:" << std::endl;
            for (const auto& failure : test_failures_) {
                std::cout << "  - " << failure << std::endl;
            }
        }
        
        std::cout << "\n" << (test_passed_ ? "✅ ALL TESTS PASSED" : "❌ SOME TESTS FAILED") << std::endl;
    }

    void on_container_start(proton::container &c) override {
        start_time_ = std::chrono::steady_clock::now();
        c.connect(conn_url_);
        log_callback("on_container_start");
    }

    void on_connection_open(proton::connection& c) override {
        log_callback("on_connection_open");
        receiver = c.open_receiver(addr_, proton::receiver_options().credit_window(0));
    }

    void on_session_open(proton::session &s) override {
        log_callback("on_session_open");
        verify_state("on_session_open", !s.transaction_is_declared(), 
                     "Transaction should not be declared yet");
        s.transaction_declare(settle_before_discharge_);
    }

    void on_session_error(proton::session &s) override {
        log_callback("on_session_error", s.error().what());
        count_transaction_error_++;
        test_passed_ = false;
        test_failures_.push_back("Session error: " + std::string(s.error().what()));
        s.connection().close();
    }

    void on_session_transaction_declared(proton::session &s) override {
        count_transaction_declared_++;
        current_transaction_id_ = s.transaction_id();
        in_transaction_ = true;
        discharging_ = false;
        
        std::ostringstream oss;
        oss << "Transaction ID: " << current_transaction_id_;
        log_callback("on_session_transaction_declared", oss.str());
        
        verify_state("on_session_transaction_declared", 
                     s.transaction_is_declared(), 
                     "Transaction should be declared");
        verify_state("on_session_transaction_declared",
                     !current_transaction_id_.empty(),
                     "Transaction ID should not be empty");
        
        receiver.add_credit(batch_size_);
    }

    void on_session_transaction_committed(proton::session &s) override {
        count_transaction_committed_++;
        discharging_ = false;
        in_transaction_ = false;
        
        log_callback("on_session_transaction_committed");
        
        verify_state("on_session_transaction_committed",
                     !s.transaction_is_declared(),
                     "Transaction should not be declared after commit");
        
        received_count_ += current_batch_;
        current_batch_ = 0;
        current_deliveries_.clear();
        
        if (received_count_ >= total_messages_) {
            std::cout << "All messages processed, closing connection." << std::endl;
            print_summary();
            s.connection().close();
        } else {
            std::cout << "Re-declaring transaction for next batch..." << std::endl;
            s.transaction_declare(settle_before_discharge_);
        }
    }

    void on_session_transaction_aborted(proton::session &s) override {
        count_transaction_aborted_++;
        discharging_ = false;
        in_transaction_ = false;
        
        log_callback("on_session_transaction_aborted");
        
        verify_state("on_session_transaction_aborted",
                     !s.transaction_is_declared(),
                     "Transaction should not be declared after abort");
        
        current_batch_ = 0;
        current_deliveries_.clear();
        
        std::cout << "Re-declaring transaction after abort..." << std::endl;
        s.transaction_declare(settle_before_discharge_);
    }

    void on_session_transaction_error(proton::session &s) override {
        count_transaction_error_++;
        in_transaction_ = false;
        discharging_ = false;
        
        auto error = s.transaction_error();
        std::ostringstream oss;
        oss << "Error: " << (error ? error.what() : "unknown");
        log_callback("on_session_transaction_error", oss.str());
        
        verify_state("on_session_transaction_error",
                     bool(error),
                     "Transaction error should be set");
        
        test_passed_ = false;
        test_failures_.push_back("Transaction error: " + 
                                 (error ? error.what() : "unknown"));
    }

    void on_transactional_accept(proton::tracker &t) override {
        count_transactional_accept_++;
        
        std::ostringstream oss;
        oss << "Tag: " << t.tag();
        log_callback("on_transactional_accept", oss.str());
        
        verify_state("on_transactional_accept",
                     !discharging_,
                     "Should not be in DISCHARGING state");
        verify_state("on_transactional_accept",
                     in_transaction_,
                     "Should be in transaction");
        verify_state("on_transactional_accept",
                     !t.settled(),
                     "Tracker should not be settled yet");
        
        delivery_trackers_[t.tag()] = t;
    }

    void on_transactional_reject(proton::tracker &t) override {
        count_transactional_reject_++;
        
        std::ostringstream oss;
        oss << "Tag: " << t.tag();
        log_callback("on_transactional_reject", oss.str());
        
        verify_state("on_transactional_reject",
                     !discharging_,
                     "Should not be in DISCHARGING state");
        verify_state("on_transactional_reject",
                     in_transaction_,
                     "Should be in transaction");
        verify_state("on_transactional_reject",
                     !t.settled(),
                     "Tracker should not be settled yet");
        
        delivery_trackers_[t.tag()] = t;
    }

    void on_transactional_release(proton::tracker &t) override {
        count_transactional_release_++;
        
        std::ostringstream oss;
        oss << "Tag: " << t.tag();
        log_callback("on_transactional_release", oss.str());
        
        verify_state("on_transactional_release",
                     !discharging_,
                     "Should not be in DISCHARGING state");
        verify_state("on_transactional_release",
                     in_transaction_,
                     "Should be in transaction");
        verify_state("on_transactional_release",
                     !t.settled(),
                     "Tracker should not be settled yet");
        
        delivery_trackers_[t.tag()] = t;
    }

    void on_tracker_settle(proton::tracker &t) override {
        count_tracker_settle_++;
        
        std::ostringstream oss;
        oss << "Tag: " << t.tag() << ", Settled: " << t.settled();
        log_callback("on_tracker_settle", oss.str());
        
        verify_state("on_tracker_settle",
                     t.settled(),
                     "Tracker should be settled when callback invoked");
        
        // Verify settlement occurs after commit/abort
        if (count_transaction_committed_ > 0 || count_transaction_aborted_ > 0) {
            verify_state("on_tracker_settle",
                         count_tracker_settle_ <= (count_transaction_committed_ + count_transaction_aborted_) * batch_size_,
                         "Settlement should occur after transaction discharge");
        }
    }

    void on_delivery_settle(proton::delivery &d) override {
        count_delivery_settle_++;
        
        std::ostringstream oss;
        oss << "Tag: " << d.tag();
        log_callback("on_delivery_settle", oss.str());
    }

    void on_message(proton::delivery &d, proton::message &msg) override {
        std::ostringstream oss;
        oss << "ID: " << msg.id() << ", Body: " << msg.body();
        log_callback("on_message", oss.str());
        
        auto session = d.session();
        
        verify_state("on_message",
                     session.transaction_is_declared(),
                     "Transaction should be declared when receiving message");
        
        verify_state("on_message",
                     !d.settled(),
                     "Delivery should not be settled when message received");
        
        current_deliveries_.push_back(d);
        current_batch_++;
        
        // Apply configured disposition
        DispositionType disp = get_next_disposition();
        apply_disposition(d, disp);
        
        // Verify delivery state after disposition
        verify_state("on_message",
                     !d.settled(),
                     "Delivery should not be settled after disposition (in transaction)");
        
        if (current_batch_ == batch_size_) {
            // Batch complete - commit or abort based on configuration
            TransactionAction action = get_next_transaction_action();
            
            if (action == TransactionAction::COMMIT) {
                std::cout << "Committing transaction..." << std::endl;
                discharging_ = true;
                session.transaction_commit();
            } else {
                std::cout << "Aborting transaction..." << std::endl;
                discharging_ = true;
                session.transaction_abort();
            }
            batch_index_++;
        }
    }
};

int main(int argc, char **argv) {
    std::string conn_url = "//127.0.0.1:5672";
    std::string addr = "examples";
    int message_count = 6;
    int batch_size = 3;
    bool settle_before_discharge = false;
    std::string disposition_str = "accept"; // accept, reject, release, modify, or comma-separated mix
    std::string transaction_action_str = "commit"; // commit, abort, or comma-separated mix
    
    example::options opts(argc, argv);

    opts.add_value(conn_url, 'u', "url", "connect and send to URL", "URL");
    opts.add_value(addr, 'a', "address", "connect and send to address", "ADDR");
    opts.add_value(message_count, 'm', "messages", "number of messages to receive", "COUNT");
    opts.add_value(batch_size, 'b', "batch_size", "number of messages in each transaction", "BATCH_SIZE");
    opts.add_flag(settle_before_discharge, 's', "settle-before-discharge", "settle deliveries before transaction discharge");
    opts.add_value(disposition_str, 'd', "disposition", "disposition sequence (accept,reject,release,modify)", "SEQ");
    opts.add_value(transaction_action_str, 't', "transaction", "transaction action sequence (commit,abort)", "SEQ");

    try {
        opts.parse();

        // Parse disposition sequence
        std::vector<DispositionType> disposition_seq;
        std::istringstream disp_stream(disposition_str);
        std::string disp_item;
        while (std::getline(disp_stream, disp_item, ',')) {
            if (disp_item == "accept") {
                disposition_seq.push_back(DispositionType::ACCEPT);
            } else if (disp_item == "reject") {
                disposition_seq.push_back(DispositionType::REJECT);
            } else if (disp_item == "release") {
                disposition_seq.push_back(DispositionType::RELEASE);
            } else if (disp_item == "modify") {
                disposition_seq.push_back(DispositionType::MODIFY);
            }
        }
        
        // Parse transaction action sequence
        std::vector<TransactionAction> transaction_seq;
        std::istringstream txn_stream(transaction_action_str);
        std::string txn_item;
        while (std::getline(txn_stream, txn_item, ',')) {
            if (txn_item == "commit") {
                transaction_seq.push_back(TransactionAction::COMMIT);
            } else if (txn_item == "abort") {
                transaction_seq.push_back(TransactionAction::ABORT);
            }
        }

        tx_recv_tester tester(conn_url, addr, message_count, batch_size, settle_before_discharge);
        tester.set_disposition_sequence(disposition_seq);
        tester.set_transaction_actions(transaction_seq);
        
        std::cout << "=== Transaction Retirement Tester ===" << std::endl;
        std::cout << "URL: " << conn_url << std::endl;
        std::cout << "Address: " << addr << std::endl;
        std::cout << "Messages: " << message_count << std::endl;
        std::cout << "Batch size: " << batch_size << std::endl;
        std::cout << "Settle before discharge: " << settle_before_discharge << std::endl;
        std::cout << "Disposition sequence: " << disposition_str << std::endl;
        std::cout << "Transaction actions: " << transaction_action_str << std::endl;
        std::cout << std::endl;

        proton::container(tester).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
