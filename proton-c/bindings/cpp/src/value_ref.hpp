#ifndef VALUE_REF_HPP
#define VALUE_REF_HPP

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

#include <proton/value.hpp>

namespace proton {

/// A value_ref provides the value interface but does not own it's own pn_data_t instance,
/// it refers to one owned by somthing else such as a pn_messenger_t or pn_condition_t.
class value_ref : public value {
  public:
    value_ref(d_data_t* p) : value() {
        // Use the empty value constructor so the value will not allocate its own pn_data_t.
        // data is a pn_object, assignment will increase the refcount
        data_ = p;
    }
}

#endif // VALUE_REF_HPP
