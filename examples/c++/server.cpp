/*
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
*/

// Simple message server directly transcribed from python into modern C++
// Usage:%prog <addr_1> ... <addr_n>"

// Compile (assuming the proton headers, libraries are installed):
// g++ --std=c++0x -o server server.cpp -lqpid-proton

// C++11
#include "proton/message.h"
#include "proton/messenger.h"

// Just to test the headers themselves
#include "proton/buffer.h"
#include "proton/driver.h"
#include "proton/engine.h"
#include "proton/error.h"
#include "proton/framing.h"
#include "proton/parser.h"
#include "proton/sasl.h"
#include "proton/scanner.h"
#include "proton/util.h"


#include <string>
#include <vector>
#include <iostream>

using std::string;
using std::vector;
using std::cout;

void dispatch(pn_message_t* request, pn_message_t* response) {
  auto subject = pn_message_get_subject(request);
  string rsubject{"Re: "};
  rsubject += subject;
  pn_message_set_subject(response, rsubject.c_str());
  cout << "Dispatched " << subject << "\n";
}

int main(int argc, char* argv[])
{
    vector<const char*> args(&argv[1], &argv[argc]);

    auto mng = pn_messenger("");
    pn_messenger_start(mng);

    if (args.empty())
        args = {"//~0.0.0.0"};

    for (auto a : args) {
        if (pn_messenger_subscribe(mng, a)) {
            cout << pn_messenger_error(mng) << "\n";
            break;
        }
    }

    auto msg = pn_message();
    auto reply = pn_message();

    while (true) {
        if (pn_messenger_incoming(mng) < 10) {
            if (pn_messenger_recv(mng, 10)) {
                cout << pn_messenger_error(mng) << "\n";
                break;
            }
        }
        if (pn_messenger_incoming(mng) > 0) {
            if (pn_messenger_get(mng, msg)) {
                cout << pn_messenger_error(mng) << "\n";
            } else {
                auto reply_to = pn_message_get_reply_to(msg);
                auto cid = pn_message_get_correlation_id(msg);
                if (reply_to)
                    pn_message_set_address(reply, reply_to);
                if (cid.type == PN_NULL)
                    pn_message_set_correlation_id(reply, cid);
                dispatch(msg, reply);
                if (pn_messenger_put(mng, reply))
                    cout << pn_messenger_error(mng) << "\n";
                if (pn_messenger_send(mng))
                    cout << pn_messenger_error(mng) << "\n";
            }
        }
    }

    pn_messenger_stop(mng);
    pn_messenger_free(mng);
    return 0;
}

