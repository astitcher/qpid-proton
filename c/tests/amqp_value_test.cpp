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



#include <proton/amqp_value.h>

#include "./pn_test.hpp"

using namespace pn_test;

namespace {

/// Layout-compatible with pn_amqp_value_t; same as existing uses of pn_atom_t in varargs.
inline pn_amqp_value_t amqp(pn_atom_t a) {
  pn_amqp_value_t v;
  std::memcpy(&v, &a, sizeof(v));
  return v;
}

} // namespace

using std::string;
using Catch::Matchers::Equals;
using Catch::Matchers::StartsWith;
using Catch::Matchers::WithinRel;
using pn_test::operator==;

TEST_CASE("amqp_lists") {

  pn_amqp_list_t *list =
    pn_amqp_list_build(nullptr,
      pn_atom_invalid()
    );

  CHECK(
    to_string( pn_amqp_list_bytes (list)) ==
    R"([])");

  list =
    pn_amqp_list_build(list,
      pn_atom(),
      pn_atom(true),
      pn_atom(10u),
      "hello list world"_a,
      "thingy-thingy"_a_sym,
      pn_atom_invalid()
    );

  CHECK(
    to_string( pn_amqp_list_bytes (list)) ==
    R"([null, true, 0xa, "hello list world", :thingy-thingy])");

  pn_amqp_list_t *nested_list =
    pn_amqp_list_build(nullptr,
      pn_atom(),
      pn_atom(true),
      pn_atom(10u),
      "hello list world"_a,
      "thingy-thingy"_a_sym,
      pn_atom_invalid()
    );

  list =
    pn_amqp_list_build(list,
      pn_atom(nested_list),
      pn_atom_invalid()
    );
  pn_amqp_list_free(nested_list);

  CHECK(
    to_string( pn_amqp_list_bytes (list)) ==
    R"([null, true, 0xa, "hello list world", :thingy-thingy, [null, true, 0xa, "hello list world", :thingy-thingy]])");

  auto value = pn_amqp_value_t {};
  auto i = pn_amqp_list_iterator(list);
  CHECK( pn_amqp_list_next (&i, &value));
  CHECK(value.value_type == PN_NULL);
  CHECK( pn_amqp_list_next (&i, &value));
  CHECK(value.value_type == PN_BOOL);
  CHECK(value.u.as_bool==true);
  CHECK( pn_amqp_list_next (&i, &value));
  CHECK(value.value_type == PN_UINT);
  CHECK(value.u.as_uint==10);
  CHECK( pn_amqp_list_next (&i, &value));
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes=="hello list world"_b);
  CHECK( pn_amqp_list_next (&i, &value));
  CHECK(value.value_type == PN_SYMBOL);
  CHECK(value.u.as_bytes=="thingy-thingy"_b);
  CHECK( pn_amqp_list_next (&i, &value));
  CHECK(value.value_type == PN_LIST);
  // TODO: Hard to check list value - future work
  CHECK_FALSE( pn_amqp_list_next (&i, &value));

  // Now do something weird - treat the list as if it were a map
  auto key = pn_amqp_value_t {};
  value = pn_amqp_value_t {};
  i = pn_amqp_list_iterator(list);
  CHECK( pn_amqp_map_next (&i, &key, &value ));
  CHECK(key.value_type == PN_NULL);
  CHECK(value.value_type == PN_BOOL);
  CHECK(value.u.as_bool==true);
  CHECK( pn_amqp_map_next (&i, &key, &value));
  CHECK(key.value_type == PN_UINT);
  CHECK(key.u.as_uint==10);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes=="hello list world"_b);
  CHECK( pn_amqp_map_next (&i, &key, &value));
  CHECK(key.value_type == PN_SYMBOL);
  CHECK(key.u.as_bytes=="thingy-thingy"_b);
  CHECK(value.value_type == PN_LIST);
  // TODO: Hard to check list value - future work
  CHECK_FALSE( pn_amqp_map_next (&i, &key, &value));

  pn_amqp_list_free(list);
}

TEST_CASE("message_properties") {

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

  CHECK(
    to_string( pn_amqp_map_bytes (mprops)) ==
    R"({"hello"=true, "world"="this-is-a-string", "this"=0x65, "is"="what fun", "me"=3.14, "Margaret"=-11729})");

  mprops =
    pn_message_properties_build(mprops,
      "a"_b,   "little"_a,
      "bit"_b, "more"_a,
      pn_bytes_null
    );

  CHECK(
    to_string( pn_amqp_map_bytes (mprops)) ==
        R"({"hello"=true, "world"="this-is-a-string", "this"=0x65, "is"="what fun", "me"=3.14, "Margaret"=-11729, "a"="little", "bit"="more"})");

  auto key = pn_bytes_t {};
  auto value = pn_amqp_value_t {};
  auto i = pn_amqp_map_iterator (mprops);
  CHECK( pn_message_properties_next (&i, &key, &value));
  CHECK(key == "hello"_b);
  CHECK(value.value_type == PN_BOOL);
  CHECK(value.u.as_bool==true);
  CHECK( pn_message_properties_next (&i, &key, &value));
  CHECK(key == "world"_b);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes == "this-is-a-string"_b);
  CHECK( pn_message_properties_next (&i, &key, &value));
  CHECK(key == "this"_b);
  CHECK(value.value_type == PN_UINT);
  CHECK(value.u.as_ulong==101);
  CHECK( pn_message_properties_next (&i, &key, &value));
  CHECK(key == "is"_b);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes == "what fun"_b);
  CHECK( pn_message_properties_next (&i, &key, &value));
  CHECK(key == "me"_b);
  CHECK(value.value_type == PN_FLOAT);
  CHECK_THAT( value.u.as_float, WithinRel(3.14, 0.001));
  CHECK( pn_message_properties_next (&i, &key, &value));
  CHECK(key == "Margaret"_b);
  CHECK(value.value_type == PN_INT);
  CHECK(value.u.as_int == -11729);
  CHECK( pn_message_properties_next (&i, &key, &value));
  CHECK(key == "a"_b);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes == "little"_b);
  CHECK( pn_message_properties_next (&i, &key, &value));
  CHECK(key == "bit"_b);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes == "more"_b);
  CHECK_FALSE( pn_message_properties_next (&i, &key, &value));

  pn_amqp_map_free(mprops);
}

TEST_CASE("amqp_fields") {

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

  CHECK(
    to_string( pn_amqp_map_bytes (fields)) ==
    R"({:hello=true, :world="this-is-a-string", :this=0x65, :is="what fun", :me=3.14, :Margaret=-11729})");

  fields =
    pn_amqp_fields_build(fields,
      "a"_b,   "little"_a,
      "bit"_b, "more"_a,
      pn_bytes_null
    );

  CHECK(
    to_string( pn_amqp_map_bytes (fields)) ==
    R"({:hello=true, :world="this-is-a-string", :this=0x65, :is="what fun", :me=3.14, :Margaret=-11729, :a="little", :bit="more"})");

  auto key = pn_bytes_t {};
  auto value = pn_amqp_value_t {};

  auto i = pn_amqp_map_iterator(fields);
  CHECK( pn_amqp_fields_next (&i, &key, &value));
  CHECK(key == "hello"_b);
  CHECK(value.value_type == PN_BOOL);
  CHECK(value.u.as_bool==true);
  CHECK( pn_amqp_fields_next (&i, &key, &value));
  CHECK(key == "world"_b);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes == "this-is-a-string"_b);
  CHECK( pn_amqp_fields_next (&i, &key, &value));
  CHECK(key == "this"_b);
  CHECK(value.value_type == PN_UINT);
  CHECK(value.u.as_ulong==101);
  CHECK( pn_amqp_fields_next (&i, &key, &value));
  CHECK(key == "is"_b);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes == "what fun"_b);
  CHECK( pn_amqp_fields_next (&i, &key, &value));
  CHECK(key == "me"_b);
  CHECK(value.value_type == PN_FLOAT);
  CHECK_THAT( value.u.as_float, WithinRel(3.14, 0.001));
  CHECK( pn_amqp_fields_next (&i, &key, &value));
  CHECK(key == "Margaret"_b);
  CHECK(value.value_type == PN_INT);
  CHECK(value.u.as_int == -11729);
  CHECK( pn_amqp_fields_next (&i, &key, &value));
  CHECK(key == "a"_b);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes == "little"_b);
  CHECK( pn_amqp_fields_next (&i, &key, &value));
  CHECK(key == "bit"_b);
  CHECK(value.value_type == PN_STRING);
  CHECK(value.u.as_bytes == "more"_b);
  CHECK_FALSE( pn_amqp_fields_next (&i, &key, &value));

  pn_amqp_map_free (fields);
}

TEST_CASE("Symbol_arrays") {
  auto symbols =
        pn_amqp_symbol_array_build (nullptr,
      "one"_b,
      "two"_b,
      "three"_b,
      pn_bytes_null
    );

  CHECK(
    to_string( pn_amqp_array_bytes (symbols)) ==
    R"(@<symbol>[:one, :two, :three])"
  );

  // Test adding a long symbol:
  // test that upgrade truncates symbol that is too long as we can't change the element type once we've created an array
  auto long_str = string(300, 'x');

  symbols =
        pn_amqp_symbol_array_build (symbols,
      pn_bytes_t{long_str.length(), long_str.data()},
      pn_bytes_null
    );

  // Exactly 255 x's
  CHECK(
    to_string( pn_amqp_array_bytes (symbols)) ==
    R"(@<symbol>[:one, :two, :three, :xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx])"
  );
  auto i = pn_amqp_array_iterator(symbols);
  auto sym = pn_bytes_t{};
  CHECK( pn_amqp_symbol_array_next (&i, &sym));
  CHECK(sym == "one"_b);
  CHECK( pn_amqp_symbol_array_next (&i, &sym));
  CHECK(sym == "two"_b);
  CHECK( pn_amqp_symbol_array_next (&i, &sym));
  CHECK(sym == "three"_b);
  CHECK( pn_amqp_symbol_array_next (&i, &sym));
  CHECK(sym == pn_bytes_t{255, long_str.data()});
  CHECK_FALSE( pn_amqp_symbol_array_next (&i, &sym));

  pn_amqp_array_free(symbols);

  // Test long symbol append
  symbols =
        pn_amqp_symbol_array_build (nullptr,
      pn_bytes_t{long_str.length(), long_str.data()},
      pn_bytes_null
    );

  CHECK(
    to_string( pn_amqp_array_bytes (symbols)) ==
    R"(@<symbol>[:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx])"
  );

  symbols =
        pn_amqp_symbol_array_build (symbols,
      "one"_b,
      "two"_b,
      "three"_b,
      pn_bytes_null
    );

  CHECK(
    to_string( pn_amqp_array_bytes (symbols)) ==
    R"(@<symbol>[:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx, :one, :two, :three])"
  );

  i = pn_amqp_array_iterator(symbols);
  CHECK( pn_amqp_symbol_array_next (&i, &sym));
  CHECK(sym == pn_bytes_t{long_str.length(), long_str.data()});
  CHECK( pn_amqp_symbol_array_next (&i, &sym));
  CHECK(sym == "one"_b);
  CHECK( pn_amqp_symbol_array_next (&i, &sym));
  CHECK(sym == "two"_b);
  CHECK( pn_amqp_symbol_array_next (&i, &sym));
  CHECK(sym == "three"_b);
  CHECK_FALSE( pn_amqp_symbol_array_next (&i, &sym));

  pn_amqp_array_free(symbols);

  pn_bytes_t t[] = {"hello"_b, "there"_b, "!"_b, ""_b};

  symbols = pn_amqp_symbol_array_buildn (4, t);
  CHECK(
    to_string( pn_amqp_array_bytes (symbols)) ==
    R"(@<symbol>[:hello, :there, :"!", :""])"
  );

  pn_amqp_array_free(symbols);
}

TEST_CASE("amqp_map_build") {
  pn_amqp_list_t *inner =
      pn_amqp_list_build(nullptr, pn_atom(false), pn_atom_invalid());
  pn_amqp_map_t *map = pn_amqp_map_build(
      nullptr,
      amqp("flag"_a_sym), amqp(pn_atom(true)),
      amqp("sub"_a_sym), amqp(pn_atom(inner)),
      amqp(pn_atom_invalid()));
  pn_amqp_list_free(inner);

  CHECK(map);
  CHECK(
      to_string(pn_amqp_map_bytes(map)) ==
      R"({:flag=true, :sub=[false]})");

  pn_amqp_value_t key{};
  pn_amqp_value_t value{};
  auto i = pn_amqp_map_iterator(map);
  CHECK(pn_amqp_map_next(&i, &key, &value));
  CHECK(key.value_type == PN_SYMBOL);
  CHECK(key.u.as_bytes == "flag"_b);
  CHECK(value.value_type == PN_BOOL);
  CHECK(value.u.as_bool);
  CHECK(pn_amqp_map_next(&i, &key, &value));
  CHECK(key.value_type == PN_SYMBOL);
  CHECK(key.u.as_bytes == "sub"_b);
  CHECK(value.value_type == PN_LIST);
  CHECK_FALSE(pn_amqp_map_next(&i, &key, &value));

  pn_amqp_map_free(map);
}

TEST_CASE("amqp_null_iterators") {
  pn_amqp_value_t v{};
  pn_amqp_value_t k{}, val{};
  pn_bytes_t sym{};
  pn_bytes_t pkey{};

  auto li = pn_amqp_list_iterator(nullptr);
  CHECK_FALSE(pn_amqp_list_next(&li, &v));

  auto mi = pn_amqp_map_iterator(nullptr);
  CHECK_FALSE(pn_amqp_map_next(&mi, &k, &val));

  auto pi = pn_amqp_map_iterator(nullptr);
  CHECK_FALSE(pn_message_properties_next(&pi, &pkey, &v));

  auto fi = pn_amqp_map_iterator(nullptr);
  CHECK_FALSE(pn_amqp_fields_next(&fi, &pkey, &v));

  auto ai = pn_amqp_array_iterator(nullptr);
  CHECK_FALSE(pn_amqp_symbol_array_next(&ai, &sym));
}

TEST_CASE("symbol_array_extras") {
  CHECK(pn_amqp_symbol_array_buildn(0, nullptr) == nullptr);

  pn_amqp_array_t *symbols =
      pn_amqp_symbol_array_build(nullptr, "n"_b, "m"_b, pn_bytes_null);
  CHECK(pn_amqp_array_count(symbols) == 2);
  CHECK(
      to_string(pn_amqp_array_bytes(symbols)) ==
      R"(@<symbol>[:n, :m])");
  pn_amqp_array_free(symbols);
}

TEST_CASE("amqp_value_bytes_and_tostring") {
  pn_amqp_list_t *empty_inner =
      pn_amqp_list_build(nullptr, pn_atom_invalid());
  pn_amqp_list_t *outer =
      pn_amqp_list_build(nullptr, pn_atom(empty_inner), pn_atom_invalid());
  pn_amqp_list_free(empty_inner);

  auto i = pn_amqp_list_iterator(outer);
  pn_amqp_value_t v{};
  CHECK(pn_amqp_list_next(&i, &v));
  CHECK(v.value_type == PN_LIST);
  pn_bytes_t raw = pn_amqp_value_bytes(&v);
  CHECK(raw.size > 0);

  char *s = pn_amqp_value_tostring(&v);
  CHECK(s != nullptr);
  CHECK(string(s) == R"([])");
  free(s);

  pn_amqp_list_free(outer);
}

TEST_CASE("amqp_fields_nested_map_value") {
  pn_amqp_map_t *inner = pn_amqp_map_build(
      nullptr,
      amqp("k"_a_sym), amqp("inner-val"_a),
      amqp(pn_atom_invalid()));

  pn_amqp_map_t *fields = pn_amqp_fields_build(
      nullptr,
      "wrap"_b, amqp(pn_atom(inner)),
      pn_bytes_null);
  pn_amqp_map_free(inner);

  CHECK(
      to_string(pn_amqp_map_bytes(fields)) ==
      R"({:wrap={:k="inner-val"}})");

  pn_bytes_t key{};
  pn_amqp_value_t val{};
  auto it = pn_amqp_map_iterator(fields);
  CHECK(pn_amqp_fields_next(&it, &key, &val));
  CHECK(key == "wrap"_b);
  CHECK(val.value_type == PN_MAP);
  CHECK_FALSE(pn_amqp_fields_next(&it, &key, &val));

  pn_amqp_map_free(fields);
}
