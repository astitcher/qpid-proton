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

#include "reactor.hpp"
#include "acceptor.hpp"
#include "connector.hpp"

#include "proton/connection.hpp"
#include "proton/url.hpp"

#include "container_impl.hpp"
#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "proton_bits.hpp"
#include "proton_event.hpp"

#include <proton/connection.h>
#include <proton/reactor.h>

namespace proton {

class handler_context {
  public:
    static handler_context& get(pn_handler_t* h) {
        return *reinterpret_cast<handler_context*>(pn_handler_mem(h));
    }
    static void cleanup(pn_handler_t*) {}

    /*
     * NOTE: this call, at the transition from C to C++ is possibly
     * the biggest performance bottleneck.  "Average" clients ignore
     * 90% of these events.  Current strategy is to create the
     * messaging_event on the stack.  For success, the messaging_event
     * should be small and free of indirect malloc/free/new/delete.
     */

    static void dispatch(pn_handler_t *c_handler, pn_event_t *c_event, pn_event_type_t)
    {
        handler_context& hc(handler_context::get(c_handler));
        proton_event pevent(c_event, hc.container_);
        pevent.dispatch(*hc.handler_);
        return;
    }

    container *container_;
    proton_handler *handler_;
};

namespace {
// FIXME aconway 2016-06-07: this is not thread safe. It is sufficient for using
// default_container::schedule() inside a handler but not for inject() from
// another thread.
struct immediate_event_loop : public event_loop {
    virtual bool inject(void_function0& f) PN_CPP_OVERRIDE {
        try { f(); } catch(...) {}
        return true;
    }

#if PN_CPP_HAS_CPP11
    virtual bool inject(std::function<void()> f) PN_CPP_OVERRIDE {
        try { f(); } catch(...) {}
        return true;
    }
#endif
};
}

// Used to sniff for connector events before the reactor's global handler sees them.
class override_handler : public proton_handler
{
  public:
    internal::pn_ptr<pn_handler_t> base_handler;
    container& container_;

    override_handler(pn_handler_t *h, container& c) : base_handler(h), container_(c) {}

    virtual void on_unhandled(proton_event &pe) {
        proton_event::event_type type = pe.type();
        if (type==proton_event::EVENT_NONE) return;  // Also not from the reactor

        pn_event_t *cevent = pe.pn_event();
        pn_connection_t *conn = pn_event_connection(cevent);
        if (conn) {
            connection c(make_wrapper(conn));
            connection_context& ctx = connection_context::get(c);
            if (!ctx.container) ctx.container = &container_;
            proton_handler *oh = ctx.handler.get();
            if (oh && type != proton_event::CONNECTION_INIT) {
                // Send event to connector
                pe.dispatch(*oh);
            }
            else if (!oh && type == proton_event::CONNECTION_INIT) {
                // Newly accepted connection from lister socket
                pn_connection_set_container(conn, container_.id().c_str());
                connection_options opts = container_.server_connection_options();
                pn_acceptor_t *pnp = pn_connection_acceptor(conn);
                listener_context &lc(listener_context::get(pnp));
                opts.update(lc.get_options());
                // Unbound options don't apply to server connection
                opts.apply_bound(c);
                // Handler applied separately
                if ( opts.handler() ) container_impl::set_handler(c, opts.handler());
                ctx.event_loop.reset(new immediate_event_loop);
            }
        }
        pn_handler_dispatch(base_handler.get(), cevent, pn_event_type_t(type));
    }
};

namespace {
internal::pn_ptr<pn_handler_t> cpp_handler(proton_handler *h, container& c) {
    pn_handler_t *handler = h ? pn_handler_new(&handler_context::dispatch,
                                               sizeof(class handler_context),
                                               &handler_context::cleanup) : 0;
    if (handler) {
        handler_context &hc = handler_context::get(handler);
        hc.container_ = &c;
        hc.handler_ = h;
    }
    return internal::take_ownership(handler);
}
}

template <class T>
void container_impl::set_handler(T s, messaging_handler* mh) {
    pn_record_t *record = internal::get_attachments(unwrap(s));
    proton_handler* h = new messaging_adapter(*mh);
    container_impl& ci = static_cast<container_impl&>(s.container());
    ci.reactor_.handlers_.push_back(h);
    pn_record_set_handler(record, cpp_handler(h, ci).get());
}

// Instantiate necessary versions of set_handler
template void container_impl::set_handler(receiver, messaging_handler*);
template void container_impl::set_handler(sender, messaging_handler*);
template void container_impl::set_handler(session, messaging_handler*);

reactor::reactor(container& c): object_(pn_reactor()), container_(c) {}
reactor::~reactor() { pn_reactor_free(object_); }

void reactor::run() { pn_reactor_run(object_); }
void reactor::start() { pn_reactor_start(object_); }
bool reactor::process() { return pn_reactor_process(object_); }
void reactor::stop() { pn_reactor_stop(object_); }
void reactor::wakeup() { pn_reactor_wakeup(object_); }
bool reactor::quiesced() { return pn_reactor_quiesced(object_); }
void reactor::yield() { pn_reactor_yield(object_); }
timestamp reactor::mark() { return timestamp(pn_reactor_mark(object_)); }
timestamp reactor::now() { return timestamp(pn_reactor_now(object_)); }
std::string reactor::error_text() { return pn_error_text(pn_reactor_error(object_)); }

void acceptor::close() { pn_acceptor_close(pn_object()); }

acceptor reactor::listen(const url& url, const connection_options& opts){
    messaging_handler* mh = opts.handler();
    internal::pn_ptr<pn_handler_t> chandler;
    if (mh) {
        proton_handler* h = new messaging_adapter(*mh);
        handlers_.push_back(h);
        chandler = cpp_handler(h, container_);
    }

    return make_wrapper(pn_reactor_acceptor(object_, url.host().c_str(), url.port().c_str(), chandler.get()));
}

void reactor::schedule(int delay, proton_handler* h) {
    internal::pn_ptr<pn_handler_t> task_handler;
    if (h)
        task_handler = cpp_handler(h, container_);
    pn_reactor_schedule(object_, delay, task_handler.get());
}

class connection reactor::connection_to_host(const proton::url& url, const connection_options& opts)
{
    messaging_handler* mh = opts.handler();
    internal::pn_ptr<pn_handler_t> chandler;
    if (mh) {
        proton_handler* h = new messaging_adapter(*mh);
        handlers_.push_back(h);
        chandler = cpp_handler(h, container_);
    }

    connection conn = make_wrapper(pn_reactor_connection_to_host(object_, url.host().c_str(), url.port().c_str(), chandler.get()));
    connection_context& cc(connection_context::get(conn));
    cc.handler.reset(new connector(conn, opts, url));
    cc.event_loop.reset(new immediate_event_loop);


    return conn;
}

void reactor::pn_handler(messaging_handler* mh) {
    if (mh) {
        proton_handler* h = new messaging_adapter(*mh);
        handlers_.push_back(h);
        pn_reactor_set_handler(object_, cpp_handler(h, container_).get());
    }
}

void reactor::pn_global_handler() {
    pn_handler_t *global_handler = pn_reactor_get_global_handler(object_);
    proton_handler* oh = new override_handler(global_handler, container_);
    handlers_.push_back(oh);
    pn_reactor_set_global_handler(object_, cpp_handler(oh, container_).get());
}

void reactor::connection_handler(connection& c, messaging_handler* mh) {
    if (mh) {
        proton_handler* h = new messaging_adapter(*mh);
        handlers_.push_back(h);
        pn_record_t *record = pn_connection_attachments(unwrap(c));
        pn_record_set_handler(record, cpp_handler(h, container_).get());
    }

}
duration reactor::timeout() {
    pn_millis_t tmo = pn_reactor_get_timeout(object_);
    if (tmo == PN_MILLIS_MAX)
        return duration::FOREVER;
    return duration(tmo);
}

void reactor::timeout(duration timeout) {
    if (timeout == duration::FOREVER || timeout.milliseconds() > PN_MILLIS_MAX)
        pn_reactor_set_timeout(object_, PN_MILLIS_MAX);
    else
        pn_reactor_set_timeout(object_, timeout.milliseconds());
}

}
