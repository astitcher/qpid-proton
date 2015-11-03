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

#include "proton/data.hpp"
#include "proton/decoder.hpp"
#include "proton/value.hpp"
#include "proton/message_id.hpp"
#include "proton_bits.hpp"
#include "msg.hpp"

#include <proton/codec.h>

namespace proton {

/**@file
 *
 * Note the pn_data_t "current" node is always pointing *before* the next value
 * to be returned by the decoder.
 *
 */
static const std::string prefix("decode: ");
decode_error::decode_error(const std::string& msg) throw() : error(prefix+msg) {}

namespace {
struct save_state {
    pn_data_t* data;
    pn_handle_t handle;
    save_state(pn_data_t* d) : data(d), handle(pn_data_point(d)) {}
    ~save_state() { if (data) pn_data_restore(data, handle); }
    void cancel() { data = 0; }
};

struct narrow {
    pn_data_t* data;
    narrow(pn_data_t* d) : data(d) { pn_data_narrow(d); }
    ~narrow() { pn_data_widen(data); }
};

template <class T> T check(T result) {
    if (result < 0)
        throw decode_error("" + error_str(result));
    return result;
}

}

void decoder::decode(const char* i, size_t size) {
    save_state ss(*this);
    const char* end = i + size;
    while (i < end) {
        i += check(pn_data_decode(*this, i, end - i));
    }
}

void decoder::decode(const std::string& buffer) {
    decode(buffer.data(), buffer.size());
}

bool decoder::more() const {
    save_state ss(*this);
    return pn_data_next(*this);
}

void decoder::rewind() { ::pn_data_rewind(*this); }

void decoder::backup() { ::pn_data_prev(*this); }

data decoder::data() { return proton::data(*this); }

namespace {

void bad_type(type_id want, type_id got) {
    if (want != got)
        throw decode_error("expected "+type_name(want)+" found "+type_name(got));
}

type_id pre_get(pn_data_t* data) {
    if (!pn_data_next(data)) throw decode_error("no more data");
    type_id t = type_id(pn_data_type(data));
    if (t < 0) throw decode_error("invalid data");
    return t;
}

// Simple extract with no type conversion.
template <class T, class U> void extract(pn_data_t* data, T& value, U (*get)(pn_data_t*)) {
    save_state ss(data);
    bad_type(type_id_of<T>::value, pre_get(data));
    value = get(data);
    ss.cancel();                // No error, no rewind
}

}

void decoder::check_type(type_id want) {
    type_id got = type();
    if (want != got) bad_type(want, got);
}

type_id decoder::type() const {
    save_state ss(*this);
    return pre_get(*this);
}

decoder operator>>(decoder d, start& s) {
    save_state ss(d);
    s.type = pre_get(d);
    switch (s.type) {
      case ARRAY:
        s.size = pn_data_get_array(d);
        s.element = type_id(pn_data_get_array_type(d)); s.is_described = pn_data_is_array_described(d);
        break;
      case LIST:
        s.size = pn_data_get_list(d);
        break;
      case MAP:
        s.size = pn_data_get_map(d);
        break;
      case DESCRIBED:
        s.is_described = true;
        s.size = 1;
        break;
      default:
        throw decode_error(MSG("" << s.type << " is not a container type"));
    }
    pn_data_enter(d);
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, finish) { pn_data_exit(d); return d; }

decoder operator>>(decoder d, skip) { pn_data_next(d); return d; }

decoder operator>>(decoder d, rewind) { d.rewind(); return d; }

decoder operator>>(decoder d, data& v) {
    if (d == v) throw decode_error("extract into self");
    v.clear();
    {
        narrow n(d);
        check(pn_data_appendn(v, d, 1));
    }
    if (!pn_data_next(d)) throw decode_error("no more data");
    return d;
}

decoder operator>>(decoder d, message_id& id) {
    switch (d.type()) {
      case ULONG:
      case UUID:
      case BINARY:
      case STRING:
        return d >> id.value_;
      default:
        throw decode_error("expected one of ulong, uuid, binary or string but found " +
                           type_name(d.type()));
    };
}

decoder operator>>(decoder d, amqp_null) {
    save_state ss(d);
    bad_type(NULL_, pre_get(d));
    return d;
}

decoder operator>>(decoder d, amqp_boolean& value) {
    extract(d, value, pn_data_get_bool);
    return d;
}

decoder operator>>(decoder d, amqp_ubyte& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case UBYTE: value = pn_data_get_ubyte(d); break;
      default: bad_type(UBYTE, type_id(type_id(pn_data_type(d))));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_byte& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case BYTE: value = pn_data_get_byte(d); break;
      default: bad_type(BYTE, type_id(type_id(pn_data_type(d))));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_ushort& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case UBYTE: value = pn_data_get_ubyte(d); break;
      case USHORT: value = pn_data_get_ushort(d); break;
      default: bad_type(USHORT, type_id(type_id(pn_data_type(d))));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_short& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case BYTE: value = pn_data_get_byte(d); break;
      case SHORT: value = pn_data_get_short(d); break;
      default: bad_type(SHORT, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_uint& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case UBYTE: value = pn_data_get_ubyte(d); break;
      case USHORT: value = pn_data_get_ushort(d); break;
      case UINT: value = pn_data_get_uint(d); break;
      default: bad_type(UINT, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_int& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case BYTE: value = pn_data_get_byte(d); break;
      case SHORT: value = pn_data_get_short(d); break;
      case INT: value = pn_data_get_int(d); break;
      default: bad_type(INT, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_ulong& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case UBYTE: value = pn_data_get_ubyte(d); break;
      case USHORT: value = pn_data_get_ushort(d); break;
      case UINT: value = pn_data_get_uint(d); break;
      case ULONG: value = pn_data_get_ulong(d); break;
      default: bad_type(ULONG, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_long& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case BYTE: value = pn_data_get_byte(d); break;
      case SHORT: value = pn_data_get_short(d); break;
      case INT: value = pn_data_get_int(d); break;
      case LONG: value = pn_data_get_long(d); break;
      default: bad_type(LONG, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_char& value) {
    extract(d, value, pn_data_get_char);
    return d;
}

decoder operator>>(decoder d, amqp_timestamp& value) {
    extract(d, value, pn_data_get_timestamp);
    return d;
}

decoder operator>>(decoder d, amqp_float& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case FLOAT: value = pn_data_get_float(d); break;
      case DOUBLE: value = pn_data_get_double(d); break;
      default: bad_type(FLOAT, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_double& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case FLOAT: value = pn_data_get_float(d); break;
      case DOUBLE: value = pn_data_get_double(d); break;
      default: bad_type(DOUBLE, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return d;
}

decoder operator>>(decoder d, amqp_decimal32& value) {
    extract(d, value, pn_data_get_decimal32);
    return d;
}

decoder operator>>(decoder d, amqp_decimal64& value) {
    extract(d, value, pn_data_get_decimal64);
    return d;
}

decoder operator>>(decoder d, amqp_decimal128& value)  {
    extract(d, value, pn_data_get_decimal128);
    return d;
}

decoder operator>>(decoder d, amqp_uuid& value)  {
    extract(d, value, pn_data_get_uuid);
    return d;
}

decoder operator>>(decoder d, std::string& value) {
    save_state ss(d);
    switch (pre_get(d)) {
      case STRING: value = str(pn_data_get_string(d)); break;
      case BINARY: value = str(pn_data_get_binary(d)); break;
      case SYMBOL: value = str(pn_data_get_symbol(d)); break;
      default: bad_type(STRING, type_id(pn_data_type(d)));
    }
    ss.cancel();
    return d;
}

}
