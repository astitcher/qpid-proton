#ifndef PROTON_MESSAGE_ID_HPP
#define PROTON_MESSAGE_ID_HPP

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

#include "./binary.hpp"
#include "./scalar_base.hpp"
#include "./uuid.hpp"

#include <proton/type_compat.h>

#include <string>

/// @file
/// @copybrief proton::message_id

namespace proton {

/// An AMQP message ID.
///
/// It can contain one of the following types:
///
///  - uint64_t
///  - std::string
///  - proton::uuid
///  - proton::binary
///
class message_id : public scalar_base {
  public:
    /// An empty message_id.
    message_id() = default;

    /// Construct from any type that can be assigned.
    template <class T> message_id(const T& x) { *this = x; }

    /// @name Assignment operators
    /// Assign a C++ value, deduce the AMQP type()
    ///
    /// @{
    message_id& operator=(uint64_t x) { put_(x); return *this; }
    message_id& operator=(const uuid& x) { put_(x); return *this; }
    message_id& operator=(const binary& x) { put_(x); return *this; }
    message_id& operator=(const std::string& x) { put_(x); return *this; }
    message_id& operator=(const char* x) { put_(x); return *this; } ///< Treated as amqp::STRING
    /// @}

  private:
    message_id(const pn_atom_t& a): scalar_base(a) {}

    ///@cond INTERNAL
  friend class message;
  friend class codec::decoder;
    ///@endcond
};

/// @cond INTERNAL
/// Base template for get(message_id), specialized for legal message_id types.
template <class T> T get(const message_id& x);
/// @endcond

/// Get the uint64_t value or throw conversion_error. @relatedalso message_id
template<> inline uint64_t get<uint64_t>(const message_id& x) { return internal::get<uint64_t>(x); }
/// Get the @ref uuid value or throw conversion_error. @relatedalso message_id
template<> inline uuid get<uuid>(const message_id& x) { return internal::get<uuid>(x); }
/// Get the @ref binary value or throw conversion_error. @relatedalso message_id
template<> inline binary get<binary>(const message_id& x) { return internal::get<binary>(x); }
/// Get the std::string value or throw conversion_error. @relatedalso message_id
template<> inline std::string get<std::string>(const message_id& x) { return internal::get<std::string>(x); }

/// @copydoc scalar::coerce
/// @relatedalso message_id
template<class T> T coerce(const message_id& x) { return internal::coerce<T>(x); }

} // proton

/// Specialize std::hash so we can use proton::message_id as a key for unordered datastructures
template <> struct std::hash<proton::message_id> {
  std::size_t operator()(const proton::message_id& s) const {
    std::size_t h;
    auto type = s.type();
    switch(type) {
      case proton::ULONG:  h = std::hash<uint64_t>{}(proton::internal::get<uint64_t>(s)); break;
      case proton::UUID:   h = std::hash<proton::uuid>{}(proton::internal::get<proton::uuid>(s)); break;
      case proton::BINARY: h = std::hash<proton::binary>{}(proton::internal::get<proton::binary>(s)); break;
      case proton::STRING: h = std::hash<std::string>{}(proton::internal::get<std::string>(s)); break;
      default: throw proton::conversion_error("invalid message_id type "+type_name(s.type()));
    }
    auto type_hash = std::hash<proton::type_id>{}(type);
    // Combine the two hashes - see https://stackoverflow.com/a/5889238
    return h ^ (type_hash + 0x9e3779b9 + (h << 6) + (h >> 2));
}

};

#endif // PROTON_MESSAGE_ID_HPP
