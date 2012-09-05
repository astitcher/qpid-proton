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
#include <cstring>
#include <vector>
#include <iostream>

using std::string;
using std::vector;
using std::cout;

using pn::Message;
using pn::Messenger;

void dispatch(const Message& request, Message& response) {
  auto subject = request.subject();
  string rsubject{"Re: "};
  rsubject += subject;
  response.subject(rsubject.c_str());
  cout << "Dispatched " << subject << "\n";
}

int main(int argc, char* argv[])
{
    vector<const char*> args(&argv[1], &argv[argc]);

    auto mng = Messenger();
    mng.start();

    if (args.empty())
        args = {"//~0.0.0.0"};

    for (auto a : args) {
        if (mng.subscribe(a)) {
            cout << mng.error() << "\n";
            break;
        }
    }

    auto msg = Message();
    auto reply = Message();

    while (true) {
        if (mng.incoming() < 10) {
            if (mng.recv(10)) {
                cout << mng.error() << "\n";
                break;
            }
        }
        if (mng.incoming() > 0) {
            if (mng.get(msg)) {
                cout << mng.error() << "\n";
            } else {
                auto reply_to = msg.replyTo();
                auto cid = msg.correlationId();
                if (reply_to)
                    reply.address(reply_to);
                if (cid.type != PN_NULL)
                    reply.correlationId(cid);
                dispatch(msg, reply);
                if (mng.put(reply))
                    cout << mng.error() << "\n";
                if (mng.send())
                    cout << mng.error() << "\n";
            }
        }
    }

    mng.stop();
    return 0;
}

