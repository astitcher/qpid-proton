/*
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
 */

#include "test_broker.hpp"
#include "proton_bits.hpp"

#include <proton/codec/decoder.hpp>
#include <proton/codec/encoder.hpp>
#include <proton/error_condition.hpp>
#include <proton/uuid.hpp>
#include <proton/value.hpp>
#include <proton/delivery.h>

#include <iostream>
#include <sstream>

// Simple debug output helper
static bool g_verbose = false;
#define DOUT(x) do {if (g_verbose) {x}} while (false)

// Transaction action implementations
namespace {
    class QueueMessageAction : public TransactionAction {
        proton::delivery delivery_;
        proton::message message_;
        std::string address_;

    public:
        QueueMessageAction(proton::delivery d, proton::message m, const std::string& addr)
            : delivery_(d), message_(m), address_(addr) {}

        void commit(TestBroker* broker) override {
            broker->publish(message_, address_);
            delivery_.settle();
        }

        void rollback(TestBroker* broker) override {
            // Nothing to do on rollback
        }
    };

    class RejectDeliveryAction : public TransactionAction {
        proton::delivery delivery_;

    public:
        RejectDeliveryAction(proton::delivery d) : delivery_(d) {}

        void commit(TestBroker* broker) override {
            delivery_.settle();
        }

        void rollback(TestBroker* broker) override {
            // Nothing to do on rollback
        }
    };

    class DeliveryUpdateAction : public TransactionAction {
        proton::delivery delivery_;
        int outcome_;

    public:
        DeliveryUpdateAction(proton::delivery d, int outcome)
            : delivery_(d), outcome_(outcome) {}

        void commit(TestBroker* broker) override {
            broker->delivery_outcome(delivery_, outcome_);
        }

        void rollback(TestBroker* broker) override {
            // Nothing to do on rollback
        }
    };
}

// Transaction implementation
void Transaction::commit(TestBroker* broker) {
    for (auto& action : actions_) {
        action->commit(broker);
    }
}

void Transaction::rollback(TestBroker* broker) {
    for (auto& action : actions_) {
        action->rollback(broker);
    }
}

// Queue implementation
void Queue::tryToSend() {
    DOUT(std::cerr << "Queue:    " << this << " tryToSend: " << subscriptions_.size(););
    size_t outOfCredit = 0;
    while (!messages_.empty() && outOfCredit < subscriptions_.size()) {
        if (current_ == subscriptions_.end()) {
            current_ = subscriptions_.begin();
        }
        auto& entry = *current_;
        Sender* sender = entry.first;
        int& credit = entry.second;
        DOUT(std::cerr << "(" << credit << ") ";);
        if (credit > 0) {
            DOUT(std::cerr << sender << " ";);
            auto msg = messages_.front();
            auto& s = sender;
            sender->add([=]{s->sendMsg(msg);});
            messages_.pop_front();
            --credit;
            ++current_;
        } else {
            ++outOfCredit;
        }
    }
    DOUT(std::cerr << "\n";);
}

void Queue::queueMsg(proton::message m) {
    DOUT(std::cerr << "Queue:    " << this << "(" << name_ << ") queueMsg\n";);
    messages_.push_back(m);
    tryToSend();
}

void Queue::flow(Sender* s, int c) {
    DOUT(std::cerr << "Queue:    " << this << "(" << name_ << ") flow: " << c << " to " << s << "\n";);
    subscriptions_[s] = c;
    tryToSend();
}

void Queue::subscribe(Sender* s) {
    DOUT(std::cerr << "Queue:    " << this << "(" << name_ << ") subscribe Sender: " << s << "\n";);
    subscriptions_[s] = 0;
}

void Queue::unsubscribe(Sender* s) {
    DOUT(std::cerr << "Queue:    " << this << "(" << name_ << ") unsubscribe Sender: " << s << "\n";);
    if (current_ != subscriptions_.end() && current_->first == s) ++current_;
    subscriptions_.erase(s);
    s->add([=]{s->unsubscribed();});
}

// Sender implementation
void Sender::on_sendable(proton::sender &sender) {
    if (queue_) {
        auto credit = sender.credit();
        queue_->add([=]{queue_->flow(this, credit);});
    } else {
        pending_credit_ = sender.credit();
    }
}

void Sender::on_sender_close(proton::sender &sender) {
    if (queue_) {
        queue_->add([=]{queue_->unsubscribe(this);});
    }
}

void Sender::on_tracker_settle(proton::tracker& tracker) {
    auto tag = tracker.tag();
    auto state = tracker.state();
    DOUT(std::cerr << "Sender:   " << this << " on_tracker_settle: " << tag << ": " << state << "\n";);
    if (auto i = unsettled_messages_.find(tag); i != unsettled_messages_.end()) {
        auto [_, msg] = *i;
        if (state == PN_RELEASED || state == PN_MODIFIED) {
            if (state == PN_MODIFIED) {
                // Note: redelivery_limit would need to be passed from broker
                // For now, we'll requeue if modified
                auto& m = msg;
                queue_->add([=] {queue_->queueMsg(m);});
            }
        }
        unsettled_messages_.erase(i);
    }
}

void Sender::boundQueue(Queue* q, std::string qn) {
    DOUT(std::cerr << "Sender:   " << this << " bound to Queue: " << q <<"(" << qn << ")\n";);
    queue_ = q;
    queue_name_ = qn;

    sender_.open(proton::sender_options()
        .source((proton::source_options().address(queue_name_)))
        .handler(*this));
    auto credit = pending_credit_;
    q->add([=]{
        q->subscribe(this);
        if (credit > 0) {
            q->flow(this, credit);
        }
    });
}

void Sender::sendMsg(proton::message m) {
    DOUT(std::cerr << "Sender:   " << this << " sending\n";);
    auto t = sender_.send(m);
    unsettled_messages_[t.tag()] = m;
}

void Sender::unsubscribed() {
    DOUT(std::cerr << "Sender:   " << this << " deleting\n";);
    sender_.user_data(nullptr);
    sender_.close();
    delete this;
}

// Receiver implementation
Receiver::Receiver(proton::receiver r, QueueManager& qm)
    : receiver_(r), work_queue_(r.work_queue()), queue_manager_(qm) {}

void Receiver::on_message(proton::delivery &d, proton::message &m) {
    auto to_address = m.to();
    if (queue_) {
        messages_.push_back(m);
        queueMsgs();
    } else if (!to_address.empty()) {
        queueMsgToNamedQueue(m, to_address);
    } else {
        d.reject();
    }
}

void Receiver::queueMsgs() {
    DOUT(std::cerr << "Receiver: " << this << " queueing " << messages_.size() << " msgs to: " << queue_ << "\n";);
    while (!messages_.empty()) {
        auto msg = messages_.front();
        queue_->add([=]{queue_->queueMsg(msg);});
        messages_.pop_front();
    }
}

void Receiver::queueMsgToNamedQueue(proton::message& m, std::string address) {
    DOUT(std::cerr << "Receiver: " << this << " send msg to Queue: " << address << "\n";);
    queue_manager_.add([=]{queue_manager_.queueMessage(m, address);});
}

void Receiver::boundQueue(Queue* q, std::string qn) {
    DOUT(std::cerr << "Receiver: " << this << " bound to Queue: " << q << "(" << qn << ")\n";);
    queue_ = q;
    receiver_.open(proton::receiver_options()
        .source((proton::source_options().address(qn)))
        .handler(*this));
    queueMsgs();
}

// QueueManager implementation
template <class T>
void QueueManager::findQueue(T& connection, std::string& qn) {
    if (qn.empty()) {
        std::ostringstream os;
        os << "_dynamic_" << next_id_++;
        qn = os.str();
    }
    Queue* q = nullptr;
    auto i = queues_.find(qn);
    if (i == queues_.end()) {
        q = new Queue(container_, qn);
        queues_[qn] = q;
    } else {
        q = i->second;
    }
    connection.add([=, &connection] {connection.boundQueue(q, qn);});
}

void QueueManager::queueMessage(proton::message m, std::string address) {
    Queue* q = nullptr;
    auto i = queues_.find(address);
    if (i == queues_.end()) {
        q = new Queue(container_, address);
        queues_[address] = q;
    } else {
        q = i->second;
    }
    q->add([=] {q->queueMsg(m);});
}

// Connection handler implementation - broker_handler removed, using direct approach

void connection_handler::on_connection_open(proton::connection& c) {
    c.open(proton::connection_options{}
        .offered_capabilities({"ANONYMOUS-RELAY"}));
}

void connection_handler::on_sender_open(proton::sender &sender) {
    std::string qn = sender.source().dynamic() ? "" : sender.source().address();
    Sender* s = new Sender(sender);
    queue_manager_.add([=]{queue_manager_.findQueueSender(s, qn);});
}

void connection_handler::on_receiver_open(proton::receiver &receiver) {
    // Check if this is a coordinator receiver by checking capabilities
    if (receiver.target().capabilities().size() > 0 &&
        receiver.target().capabilities()[0] == proton::symbol("amqp:local-transactions")) {
        // Coordinator receiver - store it and handle in on_message
        void* conn_ptr = proton::unwrap(receiver.connection());
        broker_.coordinator_receivers_[conn_ptr] = receiver;
        receiver.open();
        return;
    }
    std::string qname = receiver.target().address();
    Receiver* r = new Receiver(receiver, queue_manager_);
    if (qname.empty()) {
        receiver.open(proton::receiver_options{}
            .handler(*r));
    } else {
        queue_manager_.add([=]{queue_manager_.findQueueReceiver(r, qname);});
    }
}

void connection_handler::on_message(proton::delivery& d, proton::message& m) {
    auto receiver = d.receiver();
    void* conn_ptr = proton::unwrap(receiver.connection());
    auto coord_it = broker_.coordinator_receivers_.find(conn_ptr);
    if (coord_it != broker_.coordinator_receivers_.end() && coord_it->second == receiver) {
        broker_.handle_coordinator_message(d, m, receiver);
        return;
    }

    // Handle application messages
    auto address = receiver.target().address();
    if (address.empty()) {
        address = m.to();
    }

    // Check if this is a transactional message
    pn_delivery_t* pd = proton::unwrap(d);
    pn_disposition_t* rdisp = pn_delivery_remote(pd);
    if (rdisp && pn_disposition_type(rdisp) == PN_TRANSACTIONAL_STATE) {
        pn_transactional_disposition_t* txn_disp = pn_transactional_disposition(rdisp);
        if (txn_disp) {
            pn_bytes_t tid_bytes = pn_transactional_disposition_get_id(txn_disp);
            proton::binary tid(tid_bytes.start, tid_bytes.start + tid_bytes.size);
            Transaction* txn = broker_.find_txn(tid);
            if (txn) {
                if (!address.empty()) {
                    txn->add_action(std::make_unique<QueueMessageAction>(d, m, address));
                    // Update with transactional disposition
                    pn_disposition_t* ldisp = pn_delivery_local(pd);
                    pn_transactional_disposition_t* local_txn = pn_transactional_disposition(ldisp);
                    if (local_txn) {
                        pn_transactional_disposition_set_id(local_txn, tid_bytes);
                        pn_transactional_disposition_set_outcome_type(local_txn, PN_ACCEPTED);
                    }
                    return;
                } else {
                    txn->add_action(std::make_unique<RejectDeliveryAction>(d));
                }
            } else {
                // Unknown transaction ID
                d.reject();
                d.settle();
                return;
            }
        }
    }

    // Non-transactional message
    if (!address.empty()) {
        broker_.publish(m, address);
    }
    d.accept();
    d.settle();
}

// Note: on_delivery_updated is not a callback in the C++ API
// Delivery updates are handled via tracker.settle() callbacks in Sender

void connection_handler::on_session_close(proton::session &session) {
    for (proton::sender_iterator i = session.senders().begin(); i != session.senders().end(); ++i) {
        Sender* s = Sender::get(*i);
        if (s && s->queue_) {
            auto q = s->queue_;
            s->queue_->add([=]{q->unsubscribe(s);});
        }
    }
}

void connection_handler::on_error(const proton::error_condition& e) {
    std::cout << "protocol error: " << e.what() << std::endl;
}

void connection_handler::on_transport_close(proton::transport& t) {
    // Clean up coordinator receivers
    void* conn_ptr = proton::unwrap(t.connection());
    broker_.coordinator_receivers_.erase(conn_ptr);
    broker_.coordinator_txns_.erase(conn_ptr);
    
    for (proton::sender_iterator i = t.connection().senders().begin(); i != t.connection().senders().end(); ++i) {
        Sender* s = Sender::get(*i);
        if (s && s->queue_) {
            auto q = s->queue_;
            s->queue_->add([=]{q->unsubscribe(s);});
        }
    }
    delete this;
}

// TestBroker implementation
TestBroker::TestBroker(const std::string& address, bool verbose, unsigned redelivery_limit, double txn_timeout)
    : verbose_(verbose), redelivery_limit_(redelivery_limit), txn_timeout_(txn_timeout),
      address_(address), port_(0), container_(nullptr), queue_manager_(nullptr),
      running_(false), ready_(false) {
    g_verbose = verbose;
}

TestBroker::~TestBroker() {
    stop();
}

void TestBroker::start() {
    if (running_.exchange(true)) {
        return; // Already running
    }

    thread_ = std::thread(&TestBroker::run, this);
    wait_ready();
}

void TestBroker::stop() {
    if (!running_.exchange(false)) {
        return; // Not running
    }

    if (listener_) {
        listener_->stop();
    }
    if (container_) {
        container_->stop();
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    queue_manager_.reset();
}

void TestBroker::wait_ready(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(ready_mutex_);
    ready_cv_.wait_for(lock, timeout, [this] { return ready_; });
}

void TestBroker::run() {
    struct broker_handler : public proton::messaging_handler {
        TestBroker& broker_;
        broker_handler(TestBroker& broker) : broker_(broker) {}
        void on_container_start(proton::container& c) override {
            broker_.on_container_start(c);
        }
    };
    broker_handler handler(*this);
    container_ = std::make_unique<proton::container>(handler);
    queue_manager_ = std::make_unique<QueueManager>(*container_);
    container_->run();
}

void TestBroker::on_container_start(proton::container& c) {
    struct listener_handler : public proton::listen_handler {
        TestBroker& broker_;
        listener_handler(TestBroker& broker) : broker_(broker) {}

        proton::connection_options on_accept(proton::listener&) override {
            return proton::connection_options().handler(*(new connection_handler(*broker_.queue_manager_.get(), broker_)));
        }

        void on_open(proton::listener& l) override {
            broker_.port_ = l.port();
            std::unique_lock<std::mutex> lock(broker_.ready_mutex_);
            broker_.ready_ = true;
            broker_.ready_cv_.notify_one();
            if (broker_.verbose_) {
                std::cout << "TestBroker listening on port " << broker_.port_ << std::endl;
            }
        }

        void on_error(proton::listener&, const std::string& s) override {
            std::cerr << "listen error: " << s << std::endl;
        }
    };

    listen_handler_ = std::make_unique<listener_handler>(*this);
    listener_ = std::make_unique<proton::listener>(c.listen(address_, *listen_handler_));
}

std::string TestBroker::url() const {
    std::ostringstream os;
    os << "127.0.0.1:" << port_;
    return os.str();
}

void TestBroker::publish(proton::message m, const std::string& address) {
    if (queue_manager_) {
        queue_manager_->queueMessage(m, address);
    }
    DOUT(std::cerr << "Published message to " << address << std::endl;);
}

void TestBroker::delivery_outcome(proton::delivery d, int outcome) {
    proton::binary tag = d.tag();
    auto it = unsettled_deliveries_.find(tag);
    if (it == unsettled_deliveries_.end()) {
        return;
    }

    auto& ud = it->second;
    proton::message msg = ud.message;
    std::string address = ud.address;

    if (outcome == PN_ACCEPTED) {
        DOUT(std::cerr << "Delivery accepted" << std::endl;);
    } else if (outcome == PN_REJECTED) {
        DOUT(std::cerr << "Delivery rejected" << std::endl;);
    } else if (outcome == PN_RELEASED) {
        DOUT(std::cerr << "Delivery released, requeuing to " << address << std::endl;);
        publish(msg, address);
    } else if (outcome == PN_MODIFIED) {
        DOUT(std::cerr << "Delivery modified, requeuing to " << address << std::endl;);
        publish(msg, address);
    }

    d.settle();
    unsettled_deliveries_.erase(it);
}

void TestBroker::deliver(proton::sender consumer, proton::message message) {
    auto delivery = consumer.send(message);
    std::string address = consumer.source().address();
    proton::binary tag = delivery.tag();
    unsettled_deliveries_[tag] = {address, message};
    DOUT(std::cerr << "Delivered message to " << address << std::endl;);
}

Transaction* TestBroker::declare_txn() {
    // Generate a unique transaction ID
    proton::uuid uuid_gen = proton::uuid::random();
    // UUID is a byte_array<16>, convert to binary
    proton::binary tid(uuid_gen.begin(), uuid_gen.end());
    
    auto txn = std::make_unique<Transaction>(tid, std::chrono::steady_clock::now());
    Transaction* txn_ptr = txn.get();
    
    std::lock_guard<std::mutex> lock(txns_mutex_);
    txns_[tid] = std::move(txn);
    
    DOUT(std::cerr << "Transaction declared: " << tid << std::endl;);
    return txn_ptr;
}

void TestBroker::discharge_txn(Transaction* txn, bool failed) {
    if (!txn) return;

    proton::binary tid = txn->tid();
    if (!failed) {
        // Commit
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - txn->start_time()).count() / 1000.0;
        
        if (txn_timeout_ > 0 && elapsed > txn_timeout_) {
            DOUT(std::cerr << "Transaction timeout: " << tid << std::endl;);
            txn->rollback(this);
        } else {
            txn->commit(this);
        }
    } else {
        // Rollback
        DOUT(std::cerr << "Transaction rollback: " << tid << std::endl;);
        txn->rollback(this);
    }

    std::lock_guard<std::mutex> lock(txns_mutex_);
    txns_.erase(tid);
}

Transaction* TestBroker::find_txn(const proton::binary& tid) {
    std::lock_guard<std::mutex> lock(txns_mutex_);
    auto it = txns_.find(tid);
    return (it != txns_.end()) ? it->second.get() : nullptr;
}

void TestBroker::handle_coordinator_message(proton::delivery& d, proton::message& m, proton::receiver& coordinator) {
    proton::codec::decoder dec(m.body());
    proton::symbol descriptor;
    proton::value value;

    if (dec.next_type() == proton::type_id::DESCRIBED) {
        proton::codec::start s;
        dec >> s >> descriptor >> value >> proton::codec::finish();
    } else {
        if (verbose_) {
            std::cerr << "Invalid transaction control message format" << std::endl;
        }
        d.reject();
        d.settle();
        return;
    }

    if (descriptor == "amqp:declare:list") {
        Transaction* txn = declare_txn();
        if (txn) {
            // Set declared disposition
            pn_delivery_t* pd = proton::unwrap(d);
            pn_disposition_t* disp = pn_delivery_local(pd);
            pn_declared_disposition_t* declared = pn_declared_disposition(disp);
            if (declared) {
            proton::binary tid = txn->tid();
            pn_bytes_t tid_bytes = pn_bytes(tid.size(), reinterpret_cast<const char*>(tid.data()));
            pn_declared_disposition_set_id(declared, tid_bytes);
            }
            d.settle();
        } else {
            d.reject();
            d.settle();
        }
    } else if (descriptor == "amqp:discharge:list") {
        // Extract transaction ID and failed flag
        std::vector<proton::value> vd;
        proton::get(value, vd);
        if (vd.size() >= 2) {
            proton::binary tid = vd[0].get<proton::binary>();
            bool failed = vd[1].get<bool>();
            
            Transaction* txn = find_txn(tid);
            if (txn) {
                discharge_txn(txn, failed);
                d.accept();
            } else {
                if (verbose_) {
                    std::cerr << "Unknown transaction ID: " << tid << std::endl;
                }
                d.reject();
            }
        } else {
            d.reject();
        }
        d.settle();
    } else {
        d.reject();
        d.settle();
    }
}

