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
#include <proton/duration.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender.hpp>
#include <proton/sender_options.hpp>
#include <proton/session.hpp>
#include <proton/target_options.hpp>
#include <proton/tracker.hpp>
#include <proton/transfer.hpp>
#include <proton/types.hpp>
#include <proton/work_queue.hpp>

#include <algorithm>
#include <condition_variable>
#include <cctype>
#include <cstddef>
#include <exception>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#  define NOMINMAX
#  include <windows.h>
#else
#  include <csignal>
#endif

// Sentinel queue name: omit message 'to' address so the peer may reject the message.
constexpr std::string_view NO_TO_ADDRESS = "<none>";

// Interactive tester for AMQP transactions: declare/commit/abort transactions, receive
// messages (with optional timeout), send to the default or a given queue (or omit 'to'
// for rejection testing), wait for declare completion or disposition updates, and
// accept/reject/modify/release unsettled. Type 'help' for commands. Received messages
// are accepted on receipt.

class tx_recv_interactive : public proton::messaging_handler {
  private:
    std::string connection_url_;
    std::string receiver_addr_;

    proton::connection connection_;
    proton::receiver receiver_;
    proton::sender sender_;
    proton::session session_;
    proton::work_queue* work_queue_ = nullptr;

    mutable std::mutex wait_mutex_;
    std::condition_variable wait_cv_;
    std::string last_error_;
    std::string send_to_addr_;
    int send_pending_ = 0;
    int send_next_id_ = 0;
    int fetch_expected_ = 0;
    int fetch_received_ = 0;
    int settled_expected_ = 0;
    int settled_received_ = 0;
    bool session_ready_ = false;
    bool sleep_done_ = true;
    bool timed_out_ = false;
    bool interrupt_requested_ = false;
    bool fetch_done_ = false;
    bool settled_done_ = false;
    bool declared_done_ = false;

    std::mutex sync_mutex_;
    std::condition_variable sync_cv_;
    bool sync_done_ = false;

    /// Run f() in a try block; on error, record the message for later reporting.
    template <typename F>
    void catch_any_error(F&& f) {
        try {
            f();
        } catch (const std::exception& e) {
            auto l = std::lock_guard(wait_mutex_);
            last_error_ = e.what();
        }
    }

    void do_timeout() {
        auto l = std::lock_guard(wait_mutex_);
        timed_out_ = true;
        wait_cv_.notify_all();
    }

    // session_.transaction_declare() is only valid when no transaction is already open on this
    // session; the next declare must follow commit or abort (discharge) of the previous one.
    void do_declare() {
        {
            auto l = std::lock_guard(wait_mutex_);
            declared_done_ = false;
        }
        catch_any_error([this]() { session_.transaction_declare(); });
    }

    void do_fetch(int n) {
        receiver_.add_credit(n);
    }

    void do_commit() {
        catch_any_error([this]() { session_.transaction_commit(); });
    }

    void do_abort() {
        catch_any_error([this]() { session_.transaction_abort(); });
    }

    void do_release() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.release();
            }
        }
    }

    void do_accept() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.accept();
            }
        }
    }

    void do_reject() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.reject();
            }
        }
    }

    void do_modify() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.modify();
            }
        }
    }

    void do_quit() {
        connection_.close();
    }

    void do_send() {
        try_send();
    }

  public:
    tx_recv_interactive(const std::string& url, const std::string& addr)
        : connection_url_(url), receiver_addr_(addr) {}

    void on_container_start(proton::container& c) override {
        c.connect(connection_url_);
    }

    void on_connection_open(proton::connection& conn) override {
        connection_ = conn;
        work_queue_ = &conn.work_queue();
        // credit_window(0) so we control flow via "fetch", explicit acknowledgement
        receiver_ = conn.open_receiver(receiver_addr_, proton::receiver_options().credit_window(0).auto_accept(false));
        sender_ = conn.open_sender("", proton::sender_options{}.target(proton::target_options{}.anonymous(true)));
    }

    void on_sender_open(proton::sender& s) override {
        sender_ = s;
    }

    void on_session_open(proton::session& s) override {
        session_ = s;
        {
            auto l = std::lock_guard(wait_mutex_);
            session_ready_ = true;
        }
        wait_cv_.notify_all();
    }

    void on_session_transaction_declared(proton::session& s) override {
        std::cout << "transaction declared: " << s.transaction_id() << std::endl;
        {
            auto l = std::lock_guard(wait_mutex_);
            declared_done_ = true;
        }
        wait_cv_.notify_all();
    }

    void on_session_transaction_committed(proton::session& s) override {
        std::cout << "transaction committed" << std::endl;
    }

    void on_session_transaction_aborted(proton::session& s) override {
        std::cout << "transaction aborted: " << s.transaction_error().what() << std::endl;
    }

    void on_session_transaction_error(proton::session& s) override {
        std::cout << "transaction error: " << s.transaction_error().what() << std::endl;
        {
            auto l = std::lock_guard(wait_mutex_);
            declared_done_ = true;
        }
        wait_cv_.notify_all();
    }

    void on_sendable(proton::sender&) override {
        try_send();
    }

    void try_send() {
        int to_send = 0;
        std::string to_addr;
        {
            auto l = std::lock_guard(wait_mutex_);
            to_send = send_pending_;
            to_addr = send_to_addr_;
        }
        int sent = 0;
        while (sender_ && sender_.credit() > 0 && to_send > 0) {
            proton::message msg;
            msg.id(send_next_id_);
            if (to_addr != NO_TO_ADDRESS)
                msg.to(to_addr);
            msg.body(std::map<std::string, int>{{"message", send_next_id_}});
            sender_.send(msg);
            ++send_next_id_;
            ++sent;
            --to_send;
        }
        if (sent > 0) {
            auto l = std::lock_guard(wait_mutex_);
            send_pending_ -= sent;
            if (send_pending_ == 0)
                wait_cv_.notify_all();
        }
    }

    void on_message(proton::delivery& d, proton::message& msg) override {
        std::cout << d.tag() << ": " << msg.body() << std::endl;
        {
            auto l = std::lock_guard(wait_mutex_);
            if (fetch_expected_ > 0) {
                ++fetch_received_;
                if (fetch_received_ >= fetch_expected_) {
                    fetch_done_ = true;
                    wait_cv_.notify_all();
                }
            }
            // Pre-settled deliveries never trigger on_delivery_settle; count them here for wait_settled
            if (settled_expected_ > 0 && d.settled()) {
                ++settled_received_;
                if (settled_received_ >= settled_expected_) {
                    settled_done_ = true;
                    wait_cv_.notify_all();
                }
            }
        }
    }

    void on_delivery_settle(proton::delivery&) override {
        auto l = std::lock_guard(wait_mutex_);
        if (settled_expected_ > 0) {
            ++settled_received_;
            if (settled_received_ >= settled_expected_) {
                settled_done_ = true;
                wait_cv_.notify_all();
            }
        }
    }

    void on_transactional_accept(proton::tracker& t) override {
        std::cout << "disposition: accepted: " << t.tag() << " (transactional: " << t.session().transaction_id() << ")" << std::endl;
    }

    void on_transactional_reject(proton::tracker& t) override {
        std::cout << "disposition: rejected: " << t.tag() << " (transactional: " << t.session().transaction_id() << ")" << std::endl;
    }

    void on_transactional_release(proton::tracker& t) override {
        std::cout << "disposition: released: " << t.tag() << " (transactional: " << t.session().transaction_id() << ")" << std::endl;
    }

    void on_tracker_accept(proton::tracker& t) override {
        std::cout << "disposition: accepted: " << t.tag() << std::endl;
    }

    void on_tracker_reject(proton::tracker& t) override {
        std::cout << "disposition: rejected: " << t.tag() << std::endl;
    }

    void on_tracker_release(proton::tracker& t) override {
        std::cout << "disposition: released: " << t.tag() << std::endl;
    }

    void on_tracker_settle(proton::tracker& t) override {
        std::cout << "disposition: settled: " << t.tag() << ": " << t.state() << std::endl;
    }

    void on_session_error(proton::session& s) override {
        std::cout << "Session error: " << s.error().what() << std::endl;
        s.connection().close();
    }

    /// Called from the SIGINT-handling thread to wake any current wait.
    void request_interrupt() {
        {
            auto l = std::lock_guard(wait_mutex_);
            interrupt_requested_ = true;
            wait_cv_.notify_all();
        }
        {
            auto l = std::lock_guard(sync_mutex_);
            sync_cv_.notify_all();
        }
    }

    /// True if the last wait was exited due to Ctrl-C.
    bool interrupted() const {
        auto l = std::lock_guard(wait_mutex_);
        return interrupt_requested_;
    }

    /// Clear the interrupt flag (e.g. after the command loop has handled it).
    void clear_interrupt() {
        auto l = std::lock_guard(wait_mutex_);
        interrupt_requested_ = false;
    }

    // Thread-safe: wait until handler is ready to accept commands
    void wait_ready() {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        wait_cv_.wait(l, [this] { return session_ready_ || interrupt_requested_; });
    }

    /// Wait until the connection thread has processed all work queued so far.
    /// There is no "loop is idle" callback in the Proton API; this adds a
    /// sentinel work item and returns when it runs (so the connection thread
    /// has caught up). Use before showing the prompt so the command loop stays
    /// in sync with background connection-thread activity.
    void sync_with_connection_thread() {
        {
            auto l = std::lock_guard(wait_mutex_);
            interrupt_requested_ = false;
        }
        {
            auto l = std::lock_guard(sync_mutex_);
            sync_done_ = false;
        }
        work_queue_->add([this]() {
            auto l = std::lock_guard(sync_mutex_);
            sync_done_ = true;
            sync_cv_.notify_all();
        });
        auto l = std::unique_lock(sync_mutex_);
        sync_cv_.wait(l, [this] {
            if (sync_done_) return true;
            auto w = std::lock_guard(wait_mutex_);
            return interrupt_requested_;
        });
    }

    /// Schedule a callback on the connection thread after a delay (seconds).
    /// wait_sleep_done() blocks until that callback has run.
    void sleep(double seconds) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        sleep_done_ = false;
        l.unlock();
        auto ms = static_cast<proton::duration::numeric_type>(seconds * 1000);
        work_queue_->schedule(proton::duration(ms), [this]() {
            auto l = std::lock_guard(wait_mutex_);
            sleep_done_ = true;
            wait_cv_.notify_all();
        });
        l.lock();
        wait_cv_.wait(l, [this] { return sleep_done_ || interrupt_requested_; });
    }

    /// Start fetching n messages; optionally timeout after timeout_seconds (0 = no timeout).
    /// wait_fetch_done() blocks until n messages received or timeout (whichever first).
    void fetch(int n, double timeout_seconds) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        timed_out_ = false;
        fetch_expected_ = n;
        fetch_received_ = 0;
        fetch_done_ = (n <= 0);
        l.unlock();
        work_queue_->add([this, n]() { do_fetch(n); });
        if (timeout_seconds > 0) {
            auto ms = static_cast<proton::duration::numeric_type>(timeout_seconds * 1000);
            work_queue_->schedule(proton::duration(ms), [this]() { do_timeout(); });
        }
        l.lock();
        wait_cv_.wait(l, [this] { return fetch_done_ || timed_out_ || interrupt_requested_; });
        fetch_expected_ = 0;
    }
    int fetch_received_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return fetch_received_;
    }
    bool timed_out() const {
        auto l = std::lock_guard(wait_mutex_);
        return timed_out_;
    }

    /// Clear the timed-out flag (e.g. after the command loop has handled it).
    void clear_timed_out() {
        auto l = std::lock_guard(wait_mutex_);
        timed_out_ = false;
    }

    /// If a proton::error was recorded, assign its message to out, clear it, and return true.
    bool take_last_error(std::string& out) {
        auto l = std::lock_guard(wait_mutex_);
        if (last_error_.empty())
            return false;
        out = std::move(last_error_);
        last_error_.clear();
        return true;
    }

    /// Wait until N disposition settlements (on_delivery_settle) have been received,
    /// or timeout_seconds (0 = no timeout). Same pattern as fetch.
    void wait_settled(int n, double timeout_seconds) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        timed_out_ = false;
        settled_expected_ = n;
        settled_received_ = 0;
        settled_done_ = (n <= 0);
        l.unlock();
        if (n <= 0)
            return;
        if (timeout_seconds > 0) {
            auto ms = static_cast<proton::duration::numeric_type>(timeout_seconds * 1000);
            work_queue_->schedule(proton::duration(ms), [this]() { do_timeout(); });
        }
        l.lock();
        wait_cv_.wait(l, [this] { return settled_done_ || timed_out_ || interrupt_requested_; });
        settled_expected_ = 0;
    }
    int settled_received_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return settled_received_;
    }

    /// Wait until a declare result (on_session_transaction_declared or
    /// on_session_transaction_error) for the last do_declare() (only one in-flight is legal
    /// until commit/abort), or timeout_seconds (0 = none), or SIGINT.
    void wait_declared(double timeout_seconds) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        timed_out_ = false;
        if (declared_done_) {
            declared_done_ = false;
            return;
        }
        l.unlock();
        if (timeout_seconds > 0) {
            auto ms = static_cast<proton::duration::numeric_type>(timeout_seconds * 1000);
            work_queue_->schedule(proton::duration(ms), [this]() { do_timeout(); });
        }
        l.lock();
        wait_cv_.wait(l, [this] { return declared_done_ || timed_out_ || interrupt_requested_; });
        declared_done_ = false;
    }

    // Thread-safe: schedule on the connection thread; latch is reset in do_declare.
    void declare() {
        work_queue_->add([this]() { do_declare(); });
    }
    void commit() {
        work_queue_->add([this]() { do_commit(); });
    }
    void abort() {
        work_queue_->add([this]() { do_abort(); });
    }
    void release() {
        work_queue_->add([this]() { do_release(); });
    }
    void accept() {
        work_queue_->add([this]() { do_accept(); });
    }
    void reject() {
        work_queue_->add([this]() { do_reject(); });
    }
    void modify() {
        work_queue_->add([this]() { do_modify(); });
    }
    void send(int n, const std::string& to_addr) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        send_pending_ = n;
        send_to_addr_ = to_addr.empty() ? receiver_addr_ : to_addr;
        l.unlock();
        work_queue_->add([this]() { do_send(); });
        l.lock();
        wait_cv_.wait(l, [this] { return send_pending_ == 0 || interrupt_requested_; });
    }
    void unsettled() {
        auto count = std::size_t(0);
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                (void)d;
                ++count;
            }
        }
        std::cout << count << " unsettled delivery(ies)" << std::endl;
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                std::cout << "  " << d.tag() << std::endl;
            }
        }
    }
    void quit() {
        work_queue_->add([this]() { do_quit(); });
    }
};

using command_fn = bool (*)(tx_recv_interactive& recv, const std::vector<std::string_view>& args);

static std::optional<int> parse_int(std::string_view s) noexcept {
    try {
        return std::stoi(std::string(s));
    } catch (...) {
        return std::nullopt;
    }
}
static std::optional<double> parse_double(std::string_view s) noexcept {
    try {
        return std::stod(std::string(s));
    } catch (...) {
        return std::nullopt;
    }
}
static std::optional<std::string_view> arg(const std::vector<std::string_view>& a, size_t i) {
    if (i >= a.size()) return std::nullopt;
    return std::string_view(a[i]);
}
static bool expect_int(
    const char* cmd, const char* name, std::optional<std::string_view> text, int& out, int if_absent, int at_least) {
    if (!text) {
        out = if_absent;
        return true;
    }
    if (auto n = parse_int(*text)) {
        out = std::max(at_least, *n);
        return true;
    }
    std::cout << cmd << ": invalid " << name << " '" << *text << "'" << std::endl;
    return false;
}
static bool expect_nonneg_double(
    const char* cmd, const char* name, std::optional<std::string_view> text, double& out, double if_absent) {
    if (!text) {
        out = if_absent;
        return true;
    }
    if (auto x = parse_double(*text)) {
        out = std::max(0.0, *x);
        return true;
    }
    std::cout << cmd << ": invalid " << name << " '" << *text << "'" << std::endl;
    return false;
}

static bool cmd_abort(tx_recv_interactive& recv, const std::vector<std::string_view>&) {
    recv.abort();
    return false;
}
static bool cmd_accept(tx_recv_interactive& recv, const std::vector<std::string_view>&) {
    recv.accept();
    return false;
}
static bool cmd_commit(tx_recv_interactive& recv, const std::vector<std::string_view>&) {
    recv.commit();
    return false;
}
static bool cmd_declare(tx_recv_interactive& recv, const std::vector<std::string_view>&) {
    recv.declare();
    return false;
}
static bool cmd_fetch(tx_recv_interactive& recv, const std::vector<std::string_view>& args) {
    int n = 0;
    double timeout_seconds = 0.0;
    if (!expect_int("fetch", "message count (positive integer)", arg(args, 0), n, 1, 1)) return false;
    if (!expect_nonneg_double("fetch", "timeout in seconds (non-negative)", arg(args, 1), timeout_seconds, 0.0)) return false;
    recv.fetch(n, timeout_seconds);
    std::cout << "fetch: received " << recv.fetch_received_count() << " message(s)" << std::endl;
    return false;
}
static bool cmd_modify(tx_recv_interactive& recv, const std::vector<std::string_view>&) {
    recv.modify();
    return false;
}
static bool cmd_quit(tx_recv_interactive&, const std::vector<std::string_view>&) {
    return true;
}
static bool cmd_reject(tx_recv_interactive& recv, const std::vector<std::string_view>&) {
    recv.reject();
    return false;
}
static bool cmd_release(tx_recv_interactive& recv, const std::vector<std::string_view>&) {
    recv.release();
    return false;
}
static bool cmd_send(tx_recv_interactive& recv, const std::vector<std::string_view>& args) {
    int n = 0;
    if (!expect_int("send", "message count (positive integer)", arg(args, 0), n, 1, 1)) return false;
    std::string to_addr;
    if (args.size() >= 2) to_addr = std::string{args[1]};
    recv.send(n, to_addr);
    std::cout << "send: sent " << n << " message(s) to " << (to_addr.empty() ? "(default address)" : to_addr) << std::endl;
    return false;
}
static bool cmd_sleep(tx_recv_interactive& recv, const std::vector<std::string_view>& args) {
    if (args.empty()) {
        std::cout << "sleep: need a duration in seconds" << std::endl;
        return false;
    }
    double seconds = 0.0;
    if (!expect_nonneg_double("sleep", "duration in seconds (non-negative)", arg(args, 0), seconds, 0.0)) return false;
    recv.sleep(seconds);
    return false;
}
static bool cmd_unsettled(tx_recv_interactive& recv, const std::vector<std::string_view>&) {
    recv.unsettled();
    return false;
}
static bool cmd_wait_declared(tx_recv_interactive& recv, const std::vector<std::string_view>& args) {
    double timeout_seconds = 0.0;
    if (!expect_nonneg_double("wait_declare", "timeout in seconds (non-negative)", arg(args, 0), timeout_seconds, 0.0)) return false;
    recv.wait_declared(timeout_seconds);
    return false;
}
static bool cmd_wait_settled(tx_recv_interactive& recv, const std::vector<std::string_view>& args) {
    int n = 0;
    double timeout_seconds = 0.0;
    if (!expect_int("wait_settled", "settlement count (non-negative integer)", arg(args, 0), n, 1, 0)) return false;
    if (!expect_nonneg_double("wait_settled", "timeout in seconds (non-negative)", arg(args, 1), timeout_seconds, 0.0)) return false;
    recv.wait_settled(n, timeout_seconds);
    std::cout << "wait_settled: " << recv.settled_received_count() << " settlement(s)" << std::endl;
    return false;
}

static bool cmd_help(tx_recv_interactive&, const std::vector<std::string_view>&);

struct command_entry {
    const char* name;
    const char* description;
    command_fn fn;
};

// Lexicographically sorted by name for std::lower_bound lookup
static constexpr command_entry COMMAND_TABLE[] = {
    {"abort", "Abort the current transaction", cmd_abort},
    {"accept", "Accept all unsettled deliveries", cmd_accept},
    {"commit", "Commit the current transaction", cmd_commit},
    {"declare", "Start a transaction (not again until commit or abort)", cmd_declare},
    {"fetch", "Receive n messages (optional timeout in seconds)", cmd_fetch},
    {"help", "Show this list of commands", cmd_help},
    {"modify", "Modify all unsettled deliveries", cmd_modify},
    {"quit", "Exit the program", cmd_quit},
    {"reject", "Reject all unsettled deliveries", cmd_reject},
    {"release", "Release all unsettled deliveries", cmd_release},
    {"send", "Send n messages to queue (optional to address; use <none> to omit)", cmd_send},
    {"sleep", "Sleep for given seconds", cmd_sleep},
    {"unsettled", "List unsettled deliveries", cmd_unsettled},
    {"wait_declared", "Wait for declare to complete (optional timeout in seconds)", cmd_wait_declared},
    {"wait_settled", "Wait for n disposition updates (optional timeout)", cmd_wait_settled},
};
static constexpr std::size_t COMMAND_TABLE_SIZE = sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0]);

static bool cmd_help(tx_recv_interactive&, const std::vector<std::string_view>&) {
    for (std::size_t i = 0; i < COMMAND_TABLE_SIZE; ++i) {
        std::cout << "  " << COMMAND_TABLE[i].name << " - " << COMMAND_TABLE[i].description << "\n";
    }
    return false;
}

/// Whitespace-delimited words; each result is a view into the same buffer as `segment`.
static std::vector<std::string_view> split_words(std::string_view segment) {
    std::vector<std::string_view> out;
    const char* const base = segment.data();
    for (std::size_t i = 0; i < segment.size();) {
        while (i < segment.size() && std::isspace(static_cast<unsigned char>(segment[i]))) ++i;
        if (i >= segment.size()) break;
        auto j = i;
        while (j < segment.size() && !std::isspace(static_cast<unsigned char>(segment[j]))) ++j;
        out.emplace_back(base + i, j - i);
        i = j;
    }
    return out;
}

/// Word-split command segments for each `;`-separated part. All returned `string_view`s refer
/// to the same storage as `line` — it must not be invalidated until all uses of the return value
/// (and of anything derived from it) are finished.
static std::vector<std::vector<std::string_view>> parse_command_line(std::string_view line) {
    std::vector<std::vector<std::string_view>> commands;
    const auto n = line.size();
    std::size_t seg_start = 0;
    for (std::size_t pos = 0; pos <= n; ++pos) {
        if (pos < n && line[pos] != ';') continue;
        if (pos > seg_start) {
            auto words = split_words(line.substr(seg_start, pos - seg_start));
            if (!words.empty()) commands.push_back(std::move(words));
        }
        seg_start = pos + 1;
    }
    return commands;
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

static bool execute_command(
    tx_recv_interactive& recv, const command_entry& cmd, const std::vector<std::string_view>& all_words) {
    if (all_words.size() < 1) return false;
    return cmd.fn(
        recv, std::vector<std::string_view>(all_words.begin() + 1, all_words.end()));
}

/// Runs `;`-separated commands from `line`. `line` must stay valid for the call.
/// Returns true if the command loop should exit.
static bool run_command_line(tx_recv_interactive& recv, std::string_view line) {
    const auto commands = parse_command_line(line);
    for (const auto& all_words : commands) {
        const auto* cmd = find_command(all_words[0]);
        if (cmd) {
            if (execute_command(recv, *cmd, all_words)) {
                return true;
            }
            if (recv.interrupted()) {
                std::cout << "[interrupted]" << std::endl;
                recv.clear_interrupt();
            }
            if (recv.timed_out()) {
                std::cout << "[timed out]" << std::endl;
                recv.clear_timed_out();
            }
            recv.sync_with_connection_thread();
            if (std::string err; recv.take_last_error(err)) {
                std::cout << "[error: " << err << "]" << std::endl;
            }
        } else {
            std::cout << "Unknown command. Type 'help' for a list of commands." << std::endl;
        }
    }
    return false;
}

#if defined(_WIN32) || defined(_WIN64)
static void block_interrupt() {
}

static HANDLE g_ctrl_c_event = nullptr;

static BOOL WINAPI win_ctrl_handler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT && g_ctrl_c_event != nullptr)
        SetEvent(g_ctrl_c_event);
    return TRUE;
}

static void interrupt_thread(tx_recv_interactive* recv) {
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ev == nullptr)
        return;
    g_ctrl_c_event = ev;
    if (!SetConsoleCtrlHandler(win_ctrl_handler, TRUE)) {
        CloseHandle(ev);
        return;
    }
    for (;;) {
        if (WaitForSingleObject(ev, INFINITE) == WAIT_OBJECT_0 && recv != nullptr) {
            recv->request_interrupt();
            ResetEvent(ev);
        }
    }
}
#else
static void block_interrupt() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
}

static void interrupt_thread(tx_recv_interactive* recv) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    for (;;) {
        int sig = 0;
        if (sigwait(&set, &sig) == 0 && sig == SIGINT && recv != nullptr)
            recv->request_interrupt();
    }
}
#endif

int main(int argc, char** argv) {
    block_interrupt();

    auto conn_url = std::string("//127.0.0.1:5672");
    auto addr = std::string("examples");
    auto initial_commands = std::string();
    auto opts = example::options(argc, argv);

    opts.add_value(conn_url, 'u', "url", "connection URL", "URL");
    opts.add_value(addr, 'a', "address", "address to receive messages from", "ADDR");
    opts.add_value(initial_commands, 'c', "commands", "commands to run before interactive mode (e.g. declare; fetch 2; quit)", "COMMANDS");

    try {
        opts.parse();

        auto recv = tx_recv_interactive(conn_url, addr);
        auto container = proton::container(recv);
        auto container_thread = std::thread([&container]() { container.run(); });

        std::thread interrupt_th(interrupt_thread, &recv);
        interrupt_th.detach();

        recv.wait_ready();

        bool quit_requested = false;
        if (!initial_commands.empty()) {
            quit_requested = run_command_line(recv, initial_commands);
        }
        while (!quit_requested) {
            std::cout << "> " << std::flush;
            auto line = std::string();
            if (!std::getline(std::cin, line)) break;
            quit_requested = run_command_line(recv, line);
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
