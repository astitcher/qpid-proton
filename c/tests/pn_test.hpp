#ifndef TESTS_PN_TEST_HPP
#define TESTS_PN_TEST_HPP
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

/// @file
///
/// Wrappers and Catch2 extensions to simplify testing the proton-C library.

#include <catch_extra.hpp>

#include <proton/amqp_value.h>
#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/event.h>
#include <proton/message.h>

#include <proton/object.h>

#include <cstring>
#include <iosfwd>
#include <string>
#include <vector>

// String form of C pn_ types used in tests, must be in global C namespace
// Note objects are passed by reference, not pointer.
std::ostream &operator<<(std::ostream &, pn_event_type_t);
std::ostream &operator<<(std::ostream &, const pn_condition_t &);
std::ostream &operator<<(std::ostream &, const pn_error_t &);

namespace pn_test {

// Holder for T*, calls function Free() in dtor. Not copyable.
template <class T, void (*Free)(T *)> class auto_free {
  T *ptr_;
  auto_free &operator=(auto_free &x);
public:
  auto_free(auto_free &x) = delete;

public:
  explicit auto_free(T *p = 0) : ptr_(p) {}
  ~auto_free() { Free(ptr_); }
  T *get() const { return ptr_; }
  operator T *() const { return ptr_; } // not marking explicit for convenience
};

// Call pn_inspect(), return std::string
std::string inspect(void *);

// List of pn_event_type_t
typedef std::vector<pn_event_type_t> etypes;
std::ostream &operator<<(std::ostream &o, const etypes &et);

/// Make a pn_bytes_t from a std::string
pn_bytes_t pn_bytes(const std::string &s);

/// Ensure buf has at least size bytes, use realloc if need be
void rwbytes_ensure(pn_rwbytes_t *buf, size_t size);

/// Decode message from delivery into buf, expand buf as needed.
void message_decode(pn_message_t *m, pn_delivery_t *d, pn_rwbytes_t *buf);

// A test handler that logs the type of each event handled, and has
// slots to store all of the basic proton types for ad-hoc use in
// tests. Subclass and override the handle() method.
struct handler {
  etypes log; // Log of events
  auto_free<pn_condition_t, pn_condition_free>
      last_condition; // Condition of last event

  // Slots to save proton objects for use outside the handler.
  pn_listener_t *listener;
  pn_connection_t *connection;
  pn_session_t *session;
  pn_link_t *link;
  pn_link_t *sender;
  pn_link_t *receiver;
  pn_delivery_t *delivery;
  pn_message_t *message;

  handler();

  /// dispatch an event: log its type then call handle()
  /// Returns the value of handle()
  bool dispatch(pn_event_t *e);

  // Return the current log contents, clear the log.
  etypes log_clear();

  // Return the last event in the log, clear the log.
  pn_event_type_t log_last();

protected:
  // Override this function to handle events.
  //
  // Return true to stop dispatching and return control to the test function,
  // false to continue processing.
  virtual bool handle(pn_event_t *e) { return false; }
};

// A pn_connection_driver_t that dispatches to a pn_test::handler
//
// driver::run() dispatches events to the handler, but returns if the handler
// returns true, or if a specific event type is handled. Test functions can
// alternate between letting the handler run and checking state or calling
// functions on proton objects in the test function directly. Handlers can
// automate uninteresting work, the test function can make checks that are
// clearly located in the flow of the test logic.
struct driver : public ::pn_connection_driver_t {
  struct handler &handler;

  explicit driver(struct handler &h);
  ~driver();

  // Dispatch events till a handler returns true, the `stop` event is handled,
  // or there are no more events
  // Returns the last event handled or PN_EVENT_NONE if none were.
  pn_event_type_t run(pn_event_type_t stop = PN_EVENT_NONE);

  // Transfer available data from src write buffer to this read-buffer and
  // update both drivers. Return size of data transferred.
  size_t read(pn_connection_driver_t &src);
};

// A client/server pair drivers. run() simulates a connection in memory.
struct driver_pair {
  driver client, server;

  // Associate handlers with drivers. Sets server.transport to server mode
  driver_pair(handler &ch, handler &sh);

  // Run the drivers until a handle returns true or there is nothing left to
  // do. Opens the client.connection() if not already open.
  // Return the last event handled or PN_EVENT_NONE
  pn_event_type_t run();
};

// Matches for use with Catch macros CHECK_THAT and REQUIRE_THAT.
// Check pn_condition_t and pn_error_t, failed checks report code, name,
// description etc.

struct cond_empty : public Catch::MatcherBase<pn_condition_t> {
  std::string describe() const override;
  bool match(const pn_condition_t &cond) const override;
};

class cond_matches : public Catch::MatcherBase<pn_condition_t> {
  std::string name_, desc_;

public:
  explicit cond_matches(std::string name, std::string desc_contains = "");
  std::string describe() const override;
  bool match(const pn_condition_t &cond) const override;
};

struct error_empty : public Catch::MatcherBase<pn_error_t> {
  std::string describe() const override;
  bool match(const pn_error_t &) const override;
};

class error_matches : public Catch::MatcherBase<pn_error_t> {
  int code_;
  std::string desc_;

public:
  explicit error_matches(int code, std::string desc_contains = "");
  std::string describe() const override;
  bool match(const pn_error_t &) const override;
};

std::string to_string(pn_bytes_t raw);

static inline bool operator==(pn_bytes_t a, pn_bytes_t b) {
  return (a.size == b.size && !std::memcmp(a.start, b.start, a.size));
}

static inline pn_bytes_t operator ""_b (const char* str, size_t size) {
  return ::pn_bytes(size, str);
}

static inline pn_atom_t operator ""_a (const char* str, size_t size) {
  pn_atom_t atom;
  atom.type = PN_STRING;
  atom.u.as_bytes = ::pn_bytes(size, str);
  return atom;
}

static inline pn_atom_t operator ""_a_sym (const char* str, size_t size) {
  pn_atom_t atom;
  atom.type = PN_SYMBOL;
  atom.u.as_bytes = ::pn_bytes(size, str);
  return atom;
}

static inline pn_atom_t pn_atom() {
  pn_atom_t atom;
  atom.type = PN_NULL;
  return atom;
}

static inline pn_atom_t pn_atom(bool b) {
  pn_atom_t atom;
  atom.type = PN_BOOL;
  atom.u.as_bool = b;
  return atom;
}

static inline pn_atom_t pn_atom(const std::string& s) {
  pn_atom_t atom;
  atom.type = PN_STRING;
  atom.u.as_bytes = pn_bytes(s);
  return atom;
}

static inline pn_atom_t pn_atom_from_symbol(const std::string& s) {
  pn_atom_t atom;
  atom.type = PN_SYMBOL;
  atom.u.as_bytes = pn_bytes(s);
  return atom;
}

static inline pn_atom_t pn_atom(const pn_bytes_t bytes) {
  pn_atom_t atom;
  atom.type = PN_BINARY;
  atom.u.as_bytes = bytes;
  return atom;
}

static inline pn_atom_t pn_atom(int i) {
  pn_atom_t atom;
  atom.type = PN_INT;
  atom.u.as_int = i;
  return atom;
}

static inline pn_atom_t pn_atom(unsigned u) {
  pn_atom_t atom;
  atom.type = PN_UINT;
  atom.u.as_uint = u;
  return atom;
}

static inline pn_atom_t pn_atom(long l) {
  pn_atom_t atom;
  atom.type = PN_LONG;
  atom.u.as_long = l;
  return atom;
}

static inline pn_atom_t pn_atom(unsigned long ul) {
  pn_atom_t atom;
  atom.type = PN_ULONG;
  atom.u.as_ulong = ul;
  return atom;
}

static inline pn_atom_t pn_atom(float f) {
  pn_atom_t atom;
  atom.type = PN_FLOAT;
  atom.u.as_float = f;
  return atom;
}

static inline pn_atom_t pn_atom(double d) {
  pn_atom_t atom;
  atom.type = PN_DOUBLE;
  atom.u.as_double = d;
  return atom;
}

static inline pn_atom_t pn_atom(pn_amqp_map_t *map) {
  pn_atom_t atom;
  atom.type = PN_MAP;
  atom.u.as_bytes = pn_amqp_map_bytes (map);
  return atom;
}

static inline pn_atom_t pn_atom(pn_amqp_list_t *list) {
  pn_atom_t atom;
  atom.type = PN_LIST;
  atom.u.as_bytes = pn_amqp_list_bytes (list);
  return atom;
}

static inline pn_atom_t pn_atom_invalid() {
  pn_atom_t atom;
  atom.type = PN_INVALID;
  return atom;
}

} // namespace pn_test

#endif // TESTS_PN_TEST_HPP
