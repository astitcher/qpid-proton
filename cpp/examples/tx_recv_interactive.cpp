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
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver.hpp>
#include <proton/receiver_options.hpp>
#include <proton/session.hpp>
#include <proton/transfer.hpp>
#include <proton/work_queue.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// Interactive transaction receiver: 'declare' to start a transaction,
// 'fetch' [n] to receive one or n messages, 'commit' or 'abort' to finish, 'quit' to exit.

class tx_recv_interactive : public proton::messaging_handler {
  private:
    std::string conn_url_;
    std::string addr_;

    proton::connection connection_;
    proton::receiver receiver_;
    proton::session session_;
    proton::work_queue* work_queue_ = nullptr;

    std::mutex lock_;
    std::condition_variable ready_cv_;
    bool ready_ = false;

    void do_declare() {
        session_.transaction_declare();
    }

    void do_fetch(int n) {
        receiver_.add_credit(n);
    }

    void do_commit() {
        session_.transaction_commit();
    }

    void do_abort() {
        session_.transaction_abort();
    }

    void do_quit() {
        connection_.close();
    }

  public:
    tx_recv_interactive(const std::string& url, const std::string& addr)
        : conn_url_(url), addr_(addr) {}

    void on_container_start(proton::container& c) override {
        c.connect(conn_url_);
    }

    void on_connection_open(proton::connection& conn) override {
        connection_ = conn;
        work_queue_ = &conn.work_queue();
        // credit_window(0) so we control flow via "fetch"
        receiver_ = conn.open_receiver(addr_, proton::receiver_options().credit_window(0));
    }

    void on_session_open(proton::session& s) override {
        session_ = s;
        {
            auto l = std::lock_guard(lock_);
            ready_ = true;
        }
        ready_cv_.notify_all();
    }

    void on_session_transaction_declared(proton::session& s) override {
        std::cout << "transaction declared" << std::endl;
    }

    void on_session_transaction_committed(proton::session& s) override {
        std::cout << "transaction committed" << std::endl;
    }

    void on_session_transaction_aborted(proton::session& s) override {
        std::cout << "transaction aborted" << std::endl;
        for (auto rcv : s.receivers()) {
            for (auto t : rcv.unsettled_transfers()) {
                static_cast<proton::delivery&>(t).release();
            }
        }
    }

    void on_session_transaction_error(proton::session& s) override {
        std::cout << "transaction error" << std::endl;
        for (auto rcv : s.receivers()) {
            for (auto t : rcv.unsettled_transfers()) {
                static_cast<proton::delivery&>(t).release();
            }
        }
    }

    void on_message(proton::delivery& d, proton::message& msg) override {
        std::cout << d.tag() << ": " << msg.body() << std::endl;
        d.accept();
    }

    void on_session_error(proton::session& s) override {
        std::cout << "Session error: " << s.error().what() << std::endl;
        s.connection().close();
    }

    // Thread-safe: wait until handler is ready to accept commands
    void wait_ready() {
        auto l = std::unique_lock(lock_);
        ready_cv_.wait(l, [this] { return ready_; });
    }

    // Thread-safe: schedule work on the container thread
    void declare() {
        work_queue_->add(proton::make_work(&tx_recv_interactive::do_declare, this));
    }
    void fetch(int n) {
        work_queue_->add(proton::make_work(&tx_recv_interactive::do_fetch, this, n));
    }
    void commit() {
        work_queue_->add(proton::make_work(&tx_recv_interactive::do_commit, this));
    }
    void abort() {
        work_queue_->add(proton::make_work(&tx_recv_interactive::do_abort, this));
    }
    /// List pending (unsettled) deliveries using the session's receiver iterator.
    void list_pending() {
        auto count = std::size_t(0);
        for (auto rcv : session_.receivers()) {
            for (auto t : rcv.unsettled_transfers()) {
                (void)t;
                ++count;
            }
        }
        std::cout << count << " pending delivery(ies)" << std::endl;
        for (auto rcv : session_.receivers()) {
            for (auto t : rcv.unsettled_transfers()) {
                auto& d = static_cast<proton::delivery&>(t);
                std::cout << "  " << d.tag() << " disposition=" << d.state() << std::endl;
            }
        }
    }
    void quit() {
        work_queue_->add(proton::make_work(&tx_recv_interactive::do_quit, this));
    }
};

using command_fn = bool (*)(tx_recv_interactive& recv, const std::vector<std::string>& args);

static bool cmd_declare(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.declare();
    return false;
}
static bool cmd_fetch(tx_recv_interactive& recv, const std::vector<std::string>& args) {
    auto n = 1;
    if (!args.empty()) {
        try {
            n = std::stoi(args[0]);
            if (n < 1) n = 1;
        } catch (...) {
            std::cout << "fetch: expected positive number, got '" << args[0] << "'" << std::endl;
            return false;
        }
    }
    recv.fetch(n);
    return false;
}
static bool cmd_commit(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.commit();
    return false;
}
static bool cmd_abort(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.abort();
    return false;
}
static bool cmd_pending(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.list_pending();
    return false;
}
static bool cmd_quit(tx_recv_interactive&, const std::vector<std::string>&) {
    return true;
}

struct command_entry {
    const char* name;
    command_fn fn;
};

// Lexicographically sorted by name for std::lower_bound lookup
static constexpr command_entry COMMAND_TABLE[] = {
    {"abort", cmd_abort},
    {"commit", cmd_commit},
    {"declare", cmd_declare},
    {"fetch", cmd_fetch},
    {"pending", cmd_pending},
    {"quit", cmd_quit},
};
static constexpr std::size_t COMMAND_TABLE_SIZE = sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0]);

static void print_command_list(std::ostream& os) {
    for (std::size_t i = 0; i < COMMAND_TABLE_SIZE; ++i) {
        if (i) os << ", ";
        os << COMMAND_TABLE[i].name;
    }
}

static std::vector<std::string> split_args(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream is(line);
    for (std::string word; is >> word;) out.push_back(word);
    return out;
}

static const command_entry* find_command(std::string_view name) {
    auto it = std::lower_bound(std::begin(COMMAND_TABLE), std::end(COMMAND_TABLE), name,
        [](const command_entry& e, std::string_view s) {
            return std::string_view(e.name) < s;
        });
    if (it != std::end(COMMAND_TABLE) && std::string_view(it->name) == name)
        return &*it;
    return nullptr;
}

static bool execute_command(tx_recv_interactive& recv, const command_entry& cmd, const std::vector<std::string>& args) {
    auto cmd_args = std::vector<std::string>(args.begin() + 1, args.end());
    return cmd.fn(recv, cmd_args);
}    

int main(int argc, char** argv) {
    auto conn_url = std::string("//127.0.0.1:5672");
    auto addr = std::string("examples");
    auto opts = example::options(argc, argv);

    opts.add_value(conn_url, 'u', "url", "connection URL", "URL");
    opts.add_value(addr, 'a', "address", "address to receive messages from", "ADDR");

    try {
        opts.parse();

        auto recv = tx_recv_interactive(conn_url, addr);
        auto container = proton::container(recv);
        auto container_thread = std::thread([&container]() { container.run(); });

        recv.wait_ready();
        std::cout << "Commands: ";
        print_command_list(std::cout);
        std::cout << " (e.g. fetch 5)" << std::endl;

        auto line = std::string();
        while (std::cout << "> " << std::flush, std::getline(std::cin, line)) {
            auto args = split_args(line);
            if (args.empty())
                continue;
            auto* cmd = find_command(args[0]);
            if (cmd) {
                if (execute_command(recv, *cmd, args))
                    break;
            } else {
                std::cout << "Unknown command. Use ";
                print_command_list(std::cout);
                std::cout << "." << std::endl;
            }
        }

        recv.quit();
        container_thread.join();
        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
