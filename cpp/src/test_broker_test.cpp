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
#include <catch.hpp>

#include <proton/connection.hpp>
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver.hpp>
#include <proton/sender.hpp>
#include <proton/session.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <atomic>
#include <future>

// Simple test client to verify broker connectivity
class test_client : public proton::messaging_handler {
    std::string url_;
    proton::sender sender_;
    proton::receiver receiver_;
    bool connected_ = false;
    bool message_sent_ = false;
    bool message_received_ = false;

public:
    test_client(const std::string& url) : url_(url) {}

    void on_container_start(proton::container& c) override {
        c.connect(url_);
    }

    void on_connection_open(proton::connection& c) override {
        connected_ = true;
        sender_ = c.open_sender("test-queue");
        receiver_ = c.open_receiver("test-queue");
    }

    void on_sendable(proton::sender& s) override {
        if (!message_sent_) {
            proton::message msg("Hello, TestBroker!");
            s.send(msg);
            message_sent_ = true;
        }
    }

    void on_message(proton::delivery& d, proton::message& m) override {
        message_received_ = true;
        d.accept();
        d.connection().close();
    }

    bool connected() const { return connected_; }
    bool message_sent() const { return message_sent_; }
    bool message_received() const { return message_received_; }
};

TEST_CASE("TestBroker basic functionality", "[test_broker]") {
    SECTION("broker starts and stops") {
        TestBroker broker("127.0.0.1:0", false, 5, 0.0);
        broker.start();
        broker.wait_ready(std::chrono::milliseconds(5000));

        CHECK(broker.port() > 0);
        std::string url = broker.url();
        CHECK(!url.empty());

        broker.stop();
    }

    SECTION("broker provides valid URL") {
        TestBroker broker("127.0.0.1:0", false, 5, 0.0);
        broker.start();
        broker.wait_ready(std::chrono::milliseconds(5000));

        std::string url = broker.url();
        CHECK(url.find("127.0.0.1") != std::string::npos);
        CHECK(url.find(std::to_string(broker.port())) != std::string::npos);

        broker.stop();
    }

    SECTION("broker accepts connections") {
        TestBroker broker("127.0.0.1:0", false, 5, 0.0);
        broker.start();
        broker.wait_ready(std::chrono::milliseconds(5000));

        std::string url = broker.url();
        test_client client(url);
        proton::container container(client);

        // Run client in a separate thread
        std::thread client_thread([&container]() {
            container.run();
        });

        // Give client time to connect
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Stop client
        container.stop();
        if (client_thread.joinable()) {
            client_thread.join();
        }

        // Verify client connected
        CHECK(client.connected());

        broker.stop();
    }
}

// Transaction test client
class transaction_test_client : public proton::messaging_handler {
    std::string url_;
    proton::sender sender_;
    proton::receiver receiver_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> transaction_declared_{false};
    std::atomic<bool> transaction_committed_{false};
    std::atomic<bool> transaction_aborted_{false};
    std::atomic<int> messages_sent_{0};
    std::atomic<int> messages_received_{0};
    int messages_to_send_;
    bool should_commit_;
    std::promise<void> ready_promise_;
    std::promise<void> finished_promise_;

public:
    transaction_test_client(const std::string& url, int messages_to_send, bool should_commit)
        : url_(url), messages_to_send_(messages_to_send), should_commit_(should_commit) {}

    void on_container_start(proton::container& c) override {
        c.connect(url_);
    }

    void on_connection_open(proton::connection& c) override {
        connected_ = true;
        sender_ = c.open_sender("txn-queue");
        receiver_ = c.open_receiver("txn-queue");
    }

    void on_session_open(proton::session& s) override {
        // Wait for test to be ready, then declare transaction
        ready_promise_.get_future().wait();
        s.transaction_declare();
    }

    void on_sendable(proton::sender& s) override {
        // Only send messages if transaction is declared
        if (transaction_declared_) {
            send_messages();
        }
    }

    void send_messages() {
        proton::session session = sender_.session();
        while (session.transaction_is_declared() && sender_.credit() > 0 &&
               messages_sent_ < messages_to_send_) {
            proton::message msg("Transaction message " + std::to_string(messages_sent_ + 1));
            sender_.send(msg);
            messages_sent_++;
        }
        
        // After sending all messages, commit or abort
        if (messages_sent_ >= messages_to_send_ && session.transaction_is_declared()) {
            if (should_commit_) {
                session.transaction_commit();
            } else {
                session.transaction_abort();
            }
        }
    }

    void on_session_transaction_declared(proton::session& s) override {
        transaction_declared_ = true;
        send_messages();
    }

    void on_session_transaction_committed(proton::session& s) override {
        transaction_committed_ = true;
        finished_promise_.set_value();
        s.connection().close();
    }

    void on_session_transaction_aborted(proton::session& s) override {
        transaction_aborted_ = true;
        finished_promise_.set_value();
        s.connection().close();
    }

    void on_message(proton::delivery& d, proton::message& m) override {
        messages_received_++;
        d.accept();
    }

    // Control methods
    void start() { ready_promise_.set_value(); }
    void wait_finished(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        auto status = finished_promise_.get_future().wait_for(timeout);
        if (status == std::future_status::timeout) {
            FAIL("Transaction test client did not finish in time");
        }
    }

    // Getters
    bool connected() const { return connected_; }
    bool transaction_declared() const { return transaction_declared_; }
    bool transaction_committed() const { return transaction_committed_; }
    bool transaction_aborted() const { return transaction_aborted_; }
    int messages_sent() const { return messages_sent_; }
    int messages_received() const { return messages_received_; }
};

TEST_CASE("TestBroker transaction support", "[test_broker]") {
    SECTION("broker has transaction support") {
        TestBroker broker("127.0.0.1:0", false, 5, 0.0);
        broker.start();
        broker.wait_ready(std::chrono::milliseconds(5000));

        // Verify broker has transaction support methods
        CHECK(broker.port() > 0);
        CHECK(broker.txn_timeout() == 0.0);
        CHECK(broker.redelivery_limit() == 5);

        broker.stop();
    }

    SECTION("transaction declare and commit") {
        TestBroker broker("127.0.0.1:0", false, 5, 0.0);
        broker.start();
        broker.wait_ready(std::chrono::milliseconds(5000));

        CHECK(broker.port() > 0);
        std::string url = broker.url();

        const int messages_to_send = 3;
        transaction_test_client client(url, messages_to_send, true);
        proton::container container(client);

        // Run client in a separate thread
        std::thread client_thread([&container]() {
            container.run();
        });

        // Give client time to connect
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CHECK(client.connected());

        // Start the transaction
        client.start();

        // Wait for transaction to complete
        client.wait_finished();

        // Verify transaction was declared and committed
        CHECK(client.transaction_declared());
        CHECK(client.transaction_committed());
        CHECK_FALSE(client.transaction_aborted());
        CHECK(client.messages_sent() == messages_to_send);

        // Stop client
        container.stop();
        if (client_thread.joinable()) {
            client_thread.join();
        }

        broker.stop();
    }

    SECTION("transaction declare and abort") {
        TestBroker broker("127.0.0.1:0", false, 5, 0.0);
        broker.start();
        broker.wait_ready(std::chrono::milliseconds(5000));

        CHECK(broker.port() > 0);
        std::string url = broker.url();

        const int messages_to_send = 2;
        transaction_test_client client(url, messages_to_send, false);
        proton::container container(client);

        // Run client in a separate thread
        std::thread client_thread([&container]() {
            container.run();
        });

        // Give client time to connect
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CHECK(client.connected());

        // Start the transaction
        client.start();

        // Wait for transaction to complete
        client.wait_finished();

        // Verify transaction was declared and aborted
        CHECK(client.transaction_declared());
        CHECK(client.transaction_aborted());
        CHECK_FALSE(client.transaction_committed());
        CHECK(client.messages_sent() == messages_to_send);

        // Stop client
        container.stop();
        if (client_thread.joinable()) {
            client_thread.join();
        }

        broker.stop();
    }

    SECTION("transaction with multiple messages") {
        TestBroker broker("127.0.0.1:0", false, 5, 0.0);
        broker.start();
        broker.wait_ready(std::chrono::milliseconds(5000));

        CHECK(broker.port() > 0);
        std::string url = broker.url();

        const int messages_to_send = 5;
        transaction_test_client client(url, messages_to_send, true);
        proton::container container(client);

        // Run client in a separate thread
        std::thread client_thread([&container]() {
            container.run();
        });

        // Give client time to connect
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CHECK(client.connected());

        // Start the transaction
        client.start();

        // Wait for transaction to complete
        client.wait_finished();

        // Verify all messages were sent in transaction
        CHECK(client.transaction_declared());
        CHECK(client.transaction_committed());
        CHECK(client.messages_sent() == messages_to_send);

        // Stop client
        container.stop();
        if (client_thread.joinable()) {
            client_thread.join();
        }

        broker.stop();
    }
}
