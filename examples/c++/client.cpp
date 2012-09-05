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

using pn::Message;
using pn::Messenger;

int main(int argc, char* argv[])
{
    if (argc < 2)
        cout << "incorrect number of arguments\n";

    auto address = argv[1];
    auto subject = argv[2];
    auto replyto = "replies";

    auto mng = Messenger();
    mng.start();
    
    auto msg = Message();
    msg.address(address);
    msg.subject(subject);
    msg.replyTo(replyto);
    
    if (mng.put(msg))
        cout << mng.error() << "\n";
    if (mng.send())
        cout << mng.error() << "\n";

    if (replyto[0] != '/' && replyto[1] != '/')
        if (mng.recv(1))
            cout << mng.error() << "\n";
        else if (mng.get(msg))
            cout << mng.error() << "\n";
        else
            cout << msg.address() << " " <<  msg.subject() << "\n";

    mng.stop();
    return 0;
}

