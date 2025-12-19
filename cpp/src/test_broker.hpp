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

#ifndef TEST_BROKER_HPP
#define TEST_BROKER_HPP

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/error_condition.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender_options.hpp>
#include <proton/source_options.hpp>
#include <proton/target.hpp>
#include <proton/target_options.hpp>
#include <proton/tracker.hpp>
#include <proton/transport.hpp>
#include <proton/work_queue.hpp>
#include <proton/types.hpp>
#include <proton/codec/decoder.hpp>
#include <proton/codec/encoder.hpp>
#include <proton/delivery.h>  // For pn_transactional_disposition

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
class TestBroker;
class Queue;
class Sender;
class Receiver;

// Transaction action interface
class TransactionAction {
public:
    virtual ~TransactionAction() = default;
    virtual void commit(TestBroker* broker) = 0;
    virtual void rollback(TestBroker* broker) = 0;
};

// Transaction container
class Transaction {
public:
    Transaction(proton::binary tid, std::chrono::steady_clock::time_point start_time)
        : tid_(tid), start_time_(start_time) {}

    void add_action(std::unique_ptr<TransactionAction> action) {
        actions_.push_back(std::move(action));
    }

    void commit(TestBroker* broker);
    void rollback(TestBroker* broker);

    proton::binary tid() const { return tid_; }
    std::chrono::steady_clock::time_point start_time() const { return start_time_; }

private:
    proton::binary tid_;
    std::chrono::steady_clock::time_point start_time_;
    std::vector<std::unique_ptr<TransactionAction>> actions_;
};

// Queue for message routing
class Queue {
    friend class TestBroker;
    friend class Sender;

    proton::work_queue work_queue_;
    const std::string name_;
    std::deque<proton::message> messages_;
    using subscriptions = std::unordered_map<Sender*, int>; // With credit
    subscriptions subscriptions_;
    subscriptions::iterator current_ = subscriptions_.end();

    void tryToSend();

public:
    Queue(proton::container& c, const std::string& n)
        : work_queue_(c), name_(n) {}

    bool add(proton::work f) {
        return work_queue_.add(f);
    }

    void queueMsg(proton::message m);
    void flow(Sender* s, int c);
    void subscribe(Sender* s);
    void unsubscribe(Sender* s);
};

// Sender handler
class Sender : public proton::messaging_handler {
    friend class connection_handler;
    friend class Queue;

    proton::sender sender_;
    proton::work_queue& work_queue_;
    std::unordered_map<proton::binary, proton::message> unsettled_messages_;
    std::string queue_name_;
    Queue* queue_ = nullptr;
    int pending_credit_ = 0;

    void on_sendable(proton::sender &sender) override;
    void on_sender_close(proton::sender &sender) override;
    void on_tracker_settle(proton::tracker& tracker) override;

public:
    Sender(proton::sender s)
        : sender_(s), work_queue_(s.work_queue()) {
        s.user_data(this);
    }

    bool add(proton::work f) {
        return work_queue_.add(f);
    }

    static Sender* get(const proton::sender& s) {
        return reinterpret_cast<Sender*>(s.user_data());
    }

    void boundQueue(Queue* q, std::string qn);
    void sendMsg(proton::message m);
    void unsubscribed();
};

// Receiver handler
class Receiver : public proton::messaging_handler {
    friend class connection_handler;

    proton::receiver receiver_;
    proton::work_queue& work_queue_;
    Queue* queue_ = nullptr;
    class QueueManager& queue_manager_;
    std::deque<proton::message> messages_;

    void on_message(proton::delivery &d, proton::message &m) override;
    void queueMsgs();
    void queueMsgToNamedQueue(proton::message& m, std::string address);

public:
    Receiver(proton::receiver r, class QueueManager& qm);
    bool add(proton::work f) {
        return work_queue_.add(f);
    }
    void boundQueue(Queue* q, std::string qn);
};

// Queue manager
class QueueManager {
    friend class Receiver;
    friend class TestBroker;

    proton::container& container_;
    proton::work_queue work_queue_;
    typedef std::unordered_map<std::string, Queue*> queues;
    queues queues_;
    int next_id_;

public:
    QueueManager(proton::container& c)
        : container_(c), work_queue_(c), next_id_(0) {}

    bool add(proton::work f) {
        return work_queue_.add(f);
    }

    template <class T>
    void findQueue(T& connection, std::string& qn);

    void queueMessage(proton::message m, std::string address);

    void findQueueSender(Sender* s, std::string qn) {
        findQueue(*s, qn);
    }

    void findQueueReceiver(Receiver* r, std::string qn) {
        findQueue(*r, qn);
    }
};

// Connection handler
class connection_handler : public proton::messaging_handler {
    QueueManager& queue_manager_;
    TestBroker& broker_;

public:
    connection_handler(QueueManager& qm, TestBroker& broker)
        : queue_manager_(qm), broker_(broker) {}

    void on_connection_open(proton::connection& c) override;
    void on_sender_open(proton::sender &sender) override;
    void on_receiver_open(proton::receiver &receiver) override;
    void on_message(proton::delivery& d, proton::message& m) override;
    void on_session_close(proton::session &session) override;
    void on_error(const proton::error_condition& e) override;
    void on_transport_close(proton::transport& t) override;
};

// Main test broker class
class TestBroker {
public:
    TestBroker(const std::string& address = "127.0.0.1:0",
               bool verbose = false,
               unsigned redelivery_limit = 5,
               double txn_timeout = 0.0);
    ~TestBroker();

    // Start/stop the broker
    void start();
    void stop();
    void wait_ready(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Get broker URL
    std::string url() const;
    int port() const { return port_; }

    // Broker operations (called by transaction actions)
    void publish(proton::message m, const std::string& address);
    void delivery_outcome(proton::delivery d, int outcome);
    void deliver(proton::sender consumer, proton::message message);

    // Transaction management
    Transaction* declare_txn();
    void discharge_txn(Transaction* txn, bool failed);
    Transaction* find_txn(const proton::binary& tid);

    // Accessors
    bool verbose() const { return verbose_; }
    unsigned redelivery_limit() const { return redelivery_limit_; }
    double txn_timeout() const { return txn_timeout_; }

    // For coordinator message handling
    void handle_coordinator_message(proton::delivery& d, proton::message& m, proton::receiver& coordinator);

private:
    friend class connection_handler;
    friend class Transaction;

    void run();
    void on_container_start(proton::container& c);

    bool verbose_;
    unsigned redelivery_limit_;
    double txn_timeout_;
    std::string address_;
    int port_;

    mutable     std::unique_ptr<proton::container> container_;
    std::unique_ptr<QueueManager> queue_manager_;
    std::unique_ptr<proton::listener> listener_;
    std::unique_ptr<proton::listen_handler> listen_handler_;

    // Transaction tracking
    std::unordered_map<proton::binary, std::unique_ptr<Transaction>> txns_;
    std::mutex txns_mutex_;

    // Unsettled deliveries tracking - use delivery tag as key since delivery can't be hashed
    struct UnsettledDelivery {
        std::string address;
        proton::message message;
    };
    std::unordered_map<proton::binary, UnsettledDelivery> unsettled_deliveries_;

    // Thread management
    std::thread thread_;
    std::atomic<bool> running_;
    std::condition_variable ready_cv_;
    std::mutex ready_mutex_;
    bool ready_;

    // Coordinator receivers per connection - use connection pointer as key
    std::unordered_map<void*, std::unordered_set<Transaction*>> coordinator_txns_;
    std::unordered_map<void*, proton::receiver> coordinator_receivers_;
};

#endif // TEST_BROKER_HPP

