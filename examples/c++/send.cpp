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

//usage: %prog [options] <msg_1> ... <msg_n>

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

    auto address = "//localhost";
    if (args.empty()) {
        args = {"Hello World"};
    }
    
    auto mng = Messenger();
    mng.start();
    
    auto msg = Message();
    msg.address(address);
    for (auto m : args) {
        msg.load(m, std::strlen(m));
        if (mng.put(msg)) {
            cout << mng.error() << "\n";
            break;
        }
    }

    if (mng.send()) {
        cout << mng.error() << "\n";
    } else {
        cout << "sent: ";
        for (auto m : args) {cout << m << ", ";}
        cout << "\n";
    }

    mng.stop();

    return 0;
}
