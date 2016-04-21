#ifndef INTERNAL_VALUE_HPP
#define INTERNAL_VALUE_HPP

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
namespace internal {

// proton::value is not (directly) a proton::object and can be created in different "modes"
// so it has its own internal factory.
class value_factory {
  public:
    // Make a value that refers to the data but does not make a separate copy.
    // The data refcount is increased as normal for a proton::object.
    static value refer(const codec::data& d) {
        value v;                  // Default ctor doesn't create an internal data object.
        v.data_ = codec::data(d); // Refer to existing data object.
        return v;
    }

    /// Make a Value that contains an independent copy of the data pointed to by codec::data.
    static proton::value copy(const codec::data& d) {
        if (!d || d.empty())
            return proton::value();
        proton::value v;
        v.data().copy(d);
        return v;
    }

    /// Reset a value, make it release its data object.
    /// Needed to work-around refcounting bugs in proton.
    /// Note value::clear() only clears the internal data object, it does not
    /// release it.
    static void reset(proton::value& v) { v.data_ = codec::data(); }
};

}}

#endif // INTERNAL_VALUE_HPP
