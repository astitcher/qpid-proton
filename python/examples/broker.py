#!/usr/bin/env python3
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

import collections
from dataclasses import dataclass
import optparse
import uuid

from proton import Condition, Described, Disposition, Endpoint, Terminus, TransactionalDisposition
from proton.handlers import MessagingHandler
from proton.reactor import Container


class Queue:
    def __init__(self, dynamic=False):
        self.dynamic = dynamic
        self.queue = collections.deque()
        self.consumers = []

    def subscribe(self, consumer):
        self.consumers.append(consumer)

    def unsubscribe(self, consumer):
        if consumer in self.consumers:
            self.consumers.remove(consumer)

    def removable(self):
        """
        :return: True if the queue can be removed
        """
        return len(self.consumers) == 0 and (self.dynamic or len(self.queue) == 0)

    def publish(self, message):
        self.queue.append(message)
        self.dispatch()

    def dispatch(self, consumer=None):
        if consumer:
            c = [consumer]
        else:
            c = self.consumers
        while self._deliver_to(c):
            pass

    def _deliver_to(self, consumers):
        try:
            result = False
            for c in consumers:
                if c.credit:
                    c.send(self.queue.popleft())
                    result = True
            return result
        except IndexError:  # no more messages
            return False


class TransactionAction:
    pass


class Broker(MessagingHandler):
    def __init__(self, url):
        super().__init__(auto_accept=False)
        self.url = url
        self.queues = {}
        self.txns = set()
        self.acceptor = None

    def on_start(self, event):
        self.acceptor = event.container.listen(self.url)

    def _queue(self, address, dynamic=False):
        if address not in self.queues:
            self.queues[address] = Queue(dynamic)
        return self.queues[address]

    def on_connection_opening(self, event):
        event.connection.offered_capabilities = 'ANONYMOUS-RELAY'

    def on_link_opening(self, event):
        link = event.link
        if link.is_sender:
            dynamic = link.remote_source.dynamic
            if dynamic or link.remote_source.address:
                address = str(uuid.uuid4()) if dynamic else link.remote_source.address
                link.source.address = address
                self._queue(address, dynamic).subscribe(link)
        elif link.remote_target.type == Terminus.COORDINATOR:
            # Set up transaction coordinator
            # Should check for compatible capabilities
            # requested = link.remote_target.capabilities.get_object()
            link.target.type = Terminus.COORDINATOR
            link.target.copy(link.remote_target)
        elif link.remote_target.address:
            link.target.address = link.remote_target.address

    def _unsubscribe(self, link):
        if link.source.address in self.queues:
            q = self.queues[link.source.address]
            q.unsubscribe(link)
            if q.removable():
                del q

    def _declare_txn(self):
        tid = bytes(str(uuid.uuid4()), 'UTF8')
        self.txns.add(tid)
        return tid

    def _discharge_txn(self, tid):
        self.txns.remove(tid)

    def _coordinator_message(self, msg, delivery):
        body = msg.body
        if isinstance(body, Described):
            d = body.descriptor
            if d == "amqp:declare:list":
                # Allocate transaction id
                tid = self._declare_txn()
                print(f"Declare: txn-id={tid}")
                delivery.local = TransactionalDisposition(tid)
            elif d == "amqp:discharge:list":
                # Always accept commit/abort!
                value = body.value
                tid = bytes(value[0])
                failed = bool(value[1])
                if tid in self.txns:
                    print(f"Discharge: txn-id={tid}, failed={failed}")
                    self._discharge_txn(tid)
                    delivery.update(Disposition.ACCEPTED)
                else:
                    print(f"Discharge unknown txn-id: txn-id={tid}, failed={failed}")
                    delivery.local.condition = Condition('amqp:transaction:unknown-id')
                    delivery.update(Disposition.REJECTED)
        delivery.settle()

    def on_link_closing(self, event):
        if event.link.is_sender:
            self._unsubscribe(event.link)

    def on_connection_closing(self, event):
        self.remove_stale_consumers(event.connection)

    def on_disconnected(self, event):
        self.remove_stale_consumers(event.connection)

    def remove_stale_consumers(self, connection):
        link = connection.link_head(Endpoint.REMOTE_ACTIVE)
        while link:
            if link.is_sender:
                self._unsubscribe(link)
            link = link.next(Endpoint.REMOTE_ACTIVE)

    def on_sendable(self, event):
        self._queue(event.link.source.address).dispatch(event.link)

    def on_message(self, event):
        link = event.link
        delivery = event.delivery

        msg = event.message
        if link.target.type == Terminus.COORDINATOR:
            # Deal with special transaction messages
            self._coordinator_message(msg, delivery)
            return

        address = link.target.address
        if address is None:
            address = msg.address

        # Is this a transactioned message?
        disposition = delivery.remote
        if disposition and disposition.type == Disposition.TRANSACTIONAL_STATE:
            tid = disposition.id
            if tid in self.txns:
                print(f"Message: txn-id={tid}")
            else:
                print(f"Message unknown txn-id: txn-id={tid}")
                delivery.local.condition = Condition('amqp:transaction:unknown-id')
                delivery.update(Disposition.REJECTED)
                delivery.settle()
                return

        self._queue(address).publish(msg)
        delivery.update(Disposition.ACCEPTED)
        delivery.settle()

    def on_accepted(self, event):
        delivery = event.delivery
        print(f"Accept: delivery={delivery}")

    def on_rejected(self, event):
        delivery = event.delivery
        print(f"Reject: delivery={delivery}")

    def on_released(self, event):
        delivery = event.delivery
        print(f"Released: delivery={delivery}")

    def on_delivery_updated(self, event):
        # Is this a transactioned delivery update?
        delivery = event.delivery
        disposition = delivery.remote
        if disposition.type == Disposition.TRANSACTIONAL_STATE:
            tid = disposition.id
            outcome = disposition.outcome_type
            if tid in self.txns:
                print(f"Delivery update: txn-id={tid} outcome={outcome}")
            else:
                print(f"Message unknown txn-id: txn-id={tid}")
                delivery.local.condition = Condition('amqp:transaction:unknown-id')
                delivery.update(Disposition.REJECTED)
                delivery.settle()
                return


def main():
    parser = optparse.OptionParser(usage="usage: %prog [options]")
    parser.add_option("-a", "--address", default="localhost:5672",
                      help="address router listens on (default %default)")
    opts, args = parser.parse_args()

    try:
        Container(Broker(opts.address)).run()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
