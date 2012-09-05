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

//usage: %prog [options] <addr_1> ... <addr_n>

#include "proton/message.h"
#include "proton/messenger.h"

#include <vector>
#include <iostream>
#include <cstring>

using std::vector;
using std::cout;

using pn::Message;
using pn::Messenger;

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
    while (true) {
        if (mng.recv(10)) {
            cout << mng.error() << "\n";
            break;
        }
        while (mng.incoming()) {
            if (mng.get(msg)) {
                cout << mng.error() << "\n";
            } else {
                char body[1024];
                size_t len = sizeof(body);
                if (msg.save(body, len)) {
                    cout << msg.error() << "\n";
                } else {
                    body[len] = 0;
                    auto subject = msg.subject() ? msg.subject() : "(no subject)";
                    cout 
                        << msg.address()
                        << ", " << subject
                        << ", " << body << "\n";
                }
            }
        }
    }

    mng.stop();
}

