/*
#!/usr/bin/python
#
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

// Simple client message sender directly transcribed from python into modern C++
// usage: %prog <addr> <subject>"
// (only difference is fixed reply header not parsed from arguments)

// Compile (assuming the proton headers, libraries are installed):
// g++ --std=c++0x -o server server.cpp -lqpid-proton

// C++11
#include "proton/message.h"
#include "proton/messenger.h"

#include <string>
#include <iostream>

using std::string;
using std::cout;

int main(int argc, char* argv[])
{
    if (argc < 2)
        cout << "incorrect number of arguments\n";

    auto address = argv[1];
    auto subject = argv[2];
    auto replyto = "replies";

    auto mng = pn_messenger("");
    pn_messenger_start(mng);
    
    auto msg = pn_message();
    pn_message_set_address(msg, address);
    pn_message_set_subject(msg, subject);
    pn_message_set_reply_to(msg, replyto);
    
    if (pn_messenger_put(mng, msg))
        cout << pn_messenger_error(mng) << "\n";
    if (pn_messenger_send(mng))
        cout << pn_messenger_error(mng) << "\n";

    if (replyto[0] != '/' && replyto[1] != '/')
        if (pn_messenger_recv(mng, 1))
            cout << pn_messenger_error(mng) << "\n";
        else if (pn_messenger_get(mng, msg))
            cout << pn_messenger_error(mng) << "\n";
        else
            cout << pn_message_get_address(msg) << " " <<  pn_message_get_subject(msg) << "\n";

    pn_messenger_stop(mng);
    pn_messenger_free(mng);
    return 0;
}

