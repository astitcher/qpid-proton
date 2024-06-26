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
# under the License
#

"""
PROTON-1709 [python] ApplicationEvent causing memory growth
"""

import array
import gc
import platform
import threading
import unittest


import proton
from proton.handlers import MessagingHandler
from proton.reactor import Container, ApplicationEvent, EventInjector


class Program(MessagingHandler):
    def __init__(self, injector):
        self.injector = injector
        self.counter = 0
        self.on_start_ = threading.Event()
        self.on_start_continue_ = threading.Event()

    def on_reactor_init(self, event):
        event.reactor.selectable(self.injector)
        self.on_start_.set()
        self.on_start_continue_.wait()

    def on_count_up(self, event):
        self.counter += 1
        gc.collect()

    def on_done(self, event):
        event.subject.stop()
        gc.collect()


class Proton1709Test(unittest.TestCase):
    @unittest.skipIf(platform.system() == 'Windows', "TODO jdanek: Test is broken on Windows")
    def test_application_event_no_object_leaks(self):
        event_types_count = len(proton.EventType.TYPES)

        injector = EventInjector()
        p = Program(injector)
        c = Container(p)
        t = threading.Thread(target=c.run)
        object_counts = array.array('L', [0] * 3)

        t.start()
        p.on_start_.wait()

        gc.collect()
        object_counts[0] = len(gc.get_objects())

        p.on_start_continue_.set()

        for i in range(100):
            injector.trigger(ApplicationEvent("count_up"))

        gc.collect()
        object_counts[1] = len(gc.get_objects())

        self.assertEqual(len(proton.EventType.TYPES), event_types_count + 1)

        injector.trigger(ApplicationEvent("done", subject=c))
        self.assertEqual(len(proton.EventType.TYPES), event_types_count + 2)

        t.join()

        gc.collect()
        object_counts[2] = len(gc.get_objects())

        self.assertEqual(p.counter, 100)

        self.assertTrue(object_counts[1] - object_counts[0] <= 300,
                        "Object counts should not be increasing too fast: {0}".format(object_counts))
        self.assertTrue(object_counts[2] - object_counts[0] <= 10,
                        "No objects should be leaking at the end: {0}".format(object_counts))
