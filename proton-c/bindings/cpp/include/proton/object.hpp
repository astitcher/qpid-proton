#ifndef OBJECT_HPP
#define OBJECT_HPP
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

#include "proton/config.hpp"

#include <memory>

namespace proton {

///@cond INTERNAL

// Default refcounting uses pn_incref, pn_decref. Other types must define
// their own incref/decref overloads.
void incref(const void*);
void decref(const void*);

///@endcond

/**
 * Base class for proton object types
 *
 * Automatically perform memory management for pn_object based types.
 */
template <class T> class object {
  public:
    typedef T ptr_type;

    object() : object_(0) {}
    object(T* o) : object_(o) { incref(object_); }
    object(const object& o) : object_(o.object_) { incref(object_); }
    ~object() { decref(object_); };

    object& operator=(const object& o) 
    { incref(o.object_); decref(object_); object_ = o.object_; return *this; }

#ifdef PN_HAS_CPP11
    // Move constructor/assignment operator
    object(object&& o) : object_(o.object_) {}
    object& operator=(object&& o)
    { decref(object_); object_ = o.object_; return *this; }
#endif

    void swap(object& o) { std::swap(object_, o.object_); }

    //operator T* () const { return object_; }
    //T* operator->() const { return object_; }
    //T& operator*() const { return *object_; }
    //operator bool() const { return !!object_; }
    bool operator!() const { return !object_; }
  public:
    T* object_;
};

template <class T, class U> bool operator==(const object<T>& o1, const object<U> o2) {
    return &o1==&o2;
}

template <class T, class U> bool operator<(const object<T>& o1, const object<U> o2) {
  return &o1<&o2;
}

}
#endif // OBJECT_HPP
