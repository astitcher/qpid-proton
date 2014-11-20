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

from cproton import *

class Wrapper(object):

    def __init__(self, impl_or_constructor, get_context):
        if callable(impl_or_constructor):
            # we are constructing a new object
            impl = impl_or_constructor()
            context = get_context(impl)
            pn_context_def(context, PYCTX, PN_PYREF)
            attrs = {}
            pn_context_set(context, PYCTX, pn_py2void(attrs))
        else:
            # we are wrapping an existing object
            impl = impl_or_constructor
            pn_incref(impl)
            context = get_context(impl)
        self.__dict__["_impl"] = impl
        self.__dict__["_context"] = context

    def __getattr__(self, name):
        ctx = self.__dict__["_context"]
        attrs = pn_void2py(pn_context_get(ctx, PYCTX))
        if name in attrs:
            return attrs[name]
        else:
            raise AttributeError(name)

    def __setattr__(self, name, value):
        ctx = self.__dict__["_context"]
        attrs = pn_void2py(pn_context_get(ctx, PYCTX))
        attrs[name] = value

    def __delattr__(self, name):
        ctx = self.__dict__["_context"]
        attrs = pn_void2py(pn_context_get(ctx, PYCTX))
        if attrs:
            del attrs[name]

    def __del__(self):
        pn_decref(self._impl)

    def __repr__(self):
        return '<%s.%s 0x%x ~ 0x%x>' % (self.__class__.__module__,
                                               self.__class__.__name__,
                                               id(self), int(self._impl))

PYCTX = int(pn_py2void(Wrapper))

# above this point is the generic wrapper code, below are examples of
# how it would be used

class Connection(Wrapper):

    def __init__(self, impl = pn_connection):
        Wrapper.__init__(self, impl, pn_connection_context)

    def _pn_session(self):
        return pn_session(self._impl)

    def session(self):
        return Session(self._pn_session)

    def session_head(self):
        return Session(pn_session_head(self._impl, 0))

class Session(Wrapper):

    def __init__(self, impl):
        Wrapper.__init__(self, impl, pn_session_context)
