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

#include "./pn_test.hpp"

#include <proton/error.h>
#include <proton/message.h>

using namespace pn_test;

TEST_CASE("message_overflow_error") {
  pn_message_t *message = pn_message();
  char buf[6];
  size_t size = 6;

  int err = pn_message_encode(message, buf, &size);
  CHECK(PN_OVERFLOW == err);
  CHECK(0 == pn_message_errno(message));
  pn_message_free(message);
}

static void recode(pn_message_t *dst, pn_message_t *src) {
  pn_rwbytes_t buf = {0};
  int size = pn_message_encode2(src, &buf);
  REQUIRE(size > 0);
  pn_message_decode(dst, buf.start, size);
  free(buf.start);
}

TEST_CASE("message_inferred") {
  pn_message_t *src = pn_message();
  pn_message_t *dst = pn_message();

  CHECK(!pn_message_is_inferred(src));
  pn_data_put_binary(pn_message_body(src), pn_bytes("hello"));
  recode(dst, src);
  CHECK(!pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  CHECK(pn_message_is_inferred(dst));

  pn_message_clear(src);
  CHECK(!pn_message_is_inferred(src));
  pn_data_put_list(pn_message_body(src));
  pn_data_enter(pn_message_body(src));
  pn_data_put_binary(pn_message_body(src), pn_bytes("hello"));
  recode(dst, src);
  CHECK(!pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  CHECK(pn_message_is_inferred(dst));

  pn_message_clear(src);
  CHECK(!pn_message_is_inferred(src));
  pn_data_put_string(pn_message_body(src), pn_bytes("hello"));
  recode(dst, src);
  CHECK(!pn_message_is_inferred(dst));
  pn_message_set_inferred(src, true);
  recode(dst, src);
  /* A value section is never considered to be inferred */
  CHECK(!pn_message_is_inferred(dst));

  pn_message_free(src);
  pn_message_free(dst);
}

TEST_CASE("message_properties_roundtrip") {

  pn_amqp_map_t *mprops =
    pn_message_properties_build(
      nullptr,
      "hello"_b,    pn_atom(true),
      "world"_b,    "this-is-a-string"_a,
      "this"_b,     pn_atom(101u),
      "is"_b,       "what fun"_a,
      "me"_b,       pn_atom(3.14f),
      "Margaret"_b, pn_atom(-11729),
      pn_bytes_null
    );
  CHECK(mprops);

  auto mprops_str = to_string(pn_amqp_map_bytes(mprops));
  CHECK(
    mprops_str ==
    R"({"hello"=true, "world"="this-is-a-string", "this"=0x65, "is"="what fun", "me"=3.14, "Margaret"=-11729})");

  pn_message_t *msg = pn_message();
  pn_message_set_properties(msg, mprops);
  pn_amqp_map_t *mprops2 = pn_message_get_properties(msg);
  pn_message_free(msg);

  CHECK(
    to_string(pn_amqp_map_bytes(mprops2)) == mprops_str);

  pn_amqp_map_free(mprops2);
  pn_amqp_map_free(mprops);
}

TEST_CASE("message_annotation_rountrip") {

  auto fields =
    pn_amqp_fields_build(
      nullptr,
      "hello"_b,    pn_atom(true),
      "world"_b,    "this-is-a-string"_a,
      "this"_b,     pn_atom(101u),
      "is"_b,       "what fun"_a,
      "me"_b,       pn_atom(3.14f),
      "Margaret"_b, pn_atom(-11729),
      pn_bytes_null
    );
  CHECK(fields);

  auto fields_str = to_string(pn_amqp_map_bytes(fields));
  CHECK(
    fields_str ==
    R"({:hello=true, :world="this-is-a-string", :this=0x65, :is="what fun", :me=3.14, :Margaret=-11729})");

  auto msg = pn_message();
  pn_message_set_annotations(msg, fields);
  auto fields2 = pn_message_get_annotations(msg);
  pn_message_free(msg);

  CHECK(
    to_string(pn_amqp_map_bytes(fields2)) == fields_str);

  pn_amqp_map_free(fields2);
  pn_amqp_map_free(fields);
}
