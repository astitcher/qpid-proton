#ifndef REACTOR_HPP
#define REACTOR_HPP

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

/// @cond INTERNAL
/// XXX remove

#include "proton/internal/pn_unique_ptr.hpp"
#include "proton/duration.hpp"
#include "proton/timestamp.hpp"

#include <list>

struct pn_reactor_t;
struct pn_handler_t;
struct pn_io_t;

namespace proton {

class connection;
class connection_options;
class container;
class acceptor;
class url;
class messaging_handler;
class proton_handler;

class reactor {
  public:
    reactor(container& c);
    ~reactor();

#if PN_CPP_HAS_DELETED_FUNCTIONS
    reactor(const reactor&) = delete;
    reactor(reactor&&) = delete;
    reactor& operator=(const reactor&) = delete;
    reactor& operator=(reactor&&) = delete;
#endif

    /** Create a new reactor. */
    static reactor create(container& c);

    /** Open a connection to url and create a receiver with source=url.path() */
    acceptor listen(const proton::url& url, const connection_options& opts);

    //class connection connection(pn_handler_t*) const;
    class connection connection_to_host(const proton::url& url, const connection_options& opts);

    void schedule(int, proton_handler*);

    void pn_global_handler();
    void pn_handler(messaging_handler* mh);
    void connection_handler(connection& c, messaging_handler* mh);

    /** Run the event loop, return when all connections and acceptors are closed. */
    void run();

    /** Start the reactor, you must call process() to process events */
    void start();

    /** Process events, return true if there are more events to process. */
    bool process();

    /** Stop the reactor, causes run() to return and process() to return false. */
    void stop();

    /// Identifier for the container
    std::string id() const;

    /// Get timeout, process() will return if there is no activity within the timeout.
    duration timeout();

    /// Set timeout, process() will return if there is no activity within the timeout.
    void timeout(duration timeout);

    timestamp mark();
    timestamp now();

    void wakeup();
    bool quiesced();
    void yield();
    std::string error_text();

private:
    pn_reactor_t* object_;

    container& container_;
        // Keep a list of all the handlers used by the container so they last as long as the container
    std::list<internal::pn_unique_ptr<proton_handler> > handlers_;

  friend class container_impl;
  friend class container_context;
};

}

/// @endcond

#endif // REACTOR_HPP
