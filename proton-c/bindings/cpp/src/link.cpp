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
#include "proton/link.hpp"
#include "proton/error.hpp"
#include "proton/connection.hpp"
#include "container_impl.hpp"
#include "msg.hpp"
#include "contexts.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {

void link::open() {
    pn_link_open(object_);
}

void link::close() {
    pn_link_close(object_);
}

sender link::sender() {
    return pn_link_is_sender(object_) ? proton::sender(object_) : proton::sender();
}

receiver link::receiver() {
    return pn_link_is_receiver(object_) ? proton::receiver(object_) : proton::receiver();
}

const sender link::sender() const {
    return pn_link_is_sender(object_) ? proton::sender(object_) : proton::sender();
}

const receiver link::receiver() const {
    return pn_link_is_receiver(object_) ? proton::receiver(object_) : proton::receiver();
}

int link::credit() const {
    return pn_link_credit(object_);
}

terminus link::source() const { return pn_link_source(object_); }
terminus link::target() const { return pn_link_target(object_); }
terminus link::remote_source() const { return pn_link_remote_source(object_); }
terminus link::remote_target() const { return pn_link_remote_target(object_); }

std::string link::name() const { return std::string(pn_link_name(object_));}

class connection link::connection() const {
    return pn_session_connection(pn_link_session(object_));
}

class session link::session() const {
    return pn_link_session(object_);
}

void link::handler(class handler &h) {
    pn_record_t *record = pn_link_attachments(object_);
    connection_context& cc(connection_context::get(connection().object_));
    counted_ptr<pn_handler_t> chandler = cc.container_impl->cpp_handler(&h);
    pn_record_set_handler(record, chandler.get());
}

void link::detach_handler() {
    pn_record_t *record = pn_link_attachments(object_);
    pn_record_set_handler(record, 0);
}

endpoint::state link::state() const { return pn_link_state(object_); }
}
