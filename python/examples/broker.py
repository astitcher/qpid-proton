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

from abc import ABC, abstractmethod
from collections import deque, namedtuple
from dataclasses import dataclass
import optparse
import uuid

from typing import Optional, Union

from proton import (Condition, Delivery, Described, Disposition, DispositionType,
                    Endpoint, Link, Sender, Message, Terminus, TransactionalDisposition)
from proton.handlers import MessagingHandler
from proton.reactor import Container


UnsettledDelivery = namedtuple('UnsettledDelivery', ['queue', 'message'])
unsettled_deliveries: dict[Delivery, UnsettledDelivery] = {}


class Queue:
    def __init__(self, dynamic=False):
        self.dynamic: bool = dynamic
        self.queue: deque[Message] = deque()
        self.consumers: list[Sender] = []

    def subscribe(self, consumer: Sender):
        self.consumers.append(consumer)

    def unsubscribe(self, consumer: Sender):
        if consumer in self.consumers:
            self.consumers.remove(consumer)

    def removable(self):
        return len(self.consumers) == 0 and (self.dynamic or len(self.queue) == 0)

    def publish(self, message: Message):
        self.queue.append(message)
        self.dispatch()

    def dispatch(self, consumer: Optional[Sender] = None):
        if consumer:
            c = [consumer]
        else:
            c = self.consumers
        while self._deliver_to(c):
            pass

    def _deliver_to(self, consumers: list[Sender]):
        try:
            result = False
            for c in consumers:
                if c.credit:
                    message = self.queue.popleft()
                    delivery = c.send(message)
                    unsettled_deliveries[delivery] = UnsettledDelivery(self, message)
                    result = True
            return result
        except IndexError:  # no more messages
            return False


class TransactionAction(ABC):
    @abstractmethod
    def commit(self, broker): ...

    @abstractmethod
    def rollback(self, broker): ...


@dataclass
class QueueMessage(TransactionAction):
    delivery: Delivery
    message: Message
    address: str

    def commit(self, broker):
        broker.publish(self.delivery, self.message, self.address)
        self.delivery.update(Disposition.ACCEPTED)
        self.delivery.settle()

    def rollback(self, broker):
        self.delivery.update(Disposition.RELEASED)
        self.delivery.settle()


@dataclass
class DeliveryUpdate(TransactionAction):
    delivery: Delivery
    outcome: DispositionType

    def commit(self, broker):
        broker.delivery_update(self.delivery, self.outcome)

    def rollback(self, broker):
        broker.delivery_update(self.delivery, Disposition.RELEASED)


class Broker(MessagingHandler):
    def __init__(self, url):
        super().__init__(auto_accept=False)
        self.url = url
        self.queues: dict[str, Queue] = {}
        self.txns: dict[bytes, list[TransactionAction]] = {}
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

    def _accept_delivery(self, delivery, message, queue):
        print(f"Accepted: delivery={delivery}")
        # Delivery was accepted - nothing further to do?

    def _reject_delivery(self, delivery, message, queue):
        print(f"Rejected: delivery={delivery}")
        # Delivery was rejected - nothing further to do?

    def _release_delivery(self, delivery, message, queue):
        print(f"Released: delivery={delivery}")
        # Requeue the message from the delivery
        queue.publish(message)

    def _modify_delivery(self, delivery: Delivery, message: Message, queue: Queue):
        print(f"Modified: delivery={delivery}")
        disposition = delivery.remote
        # If not deliverable don't requeue
        if disposition.undeliverable:
            print(f" Undeliverable: delivery={delivery}")
            # Don't requeue the message
            return
        # Check if we need to update the delivery count
        if disposition.failed:
            message.delivery_count += 1
        # Requeue the message from the delivery
        queue.publish(message)

    def delivery_update(self, delivery: Delivery, outcome: Union[int, DispositionType]):
        unsettled_delivery = unsettled_deliveries[delivery]
        message = unsettled_delivery.message
        queue = unsettled_delivery.queue
        if outcome == Disposition.ACCEPTED:
            self._accept_delivery(delivery, message, queue)
        elif outcome == Disposition.REJECTED:
            self._reject_delivery(delivery, message, queue)
        elif outcome == Disposition.RELEASED:
            self._release_delivery(delivery, message, queue)
        elif outcome == Disposition.MODIFIED:
            self._modify_delivery(delivery, message, queue)
        delivery.settle()
        del unsettled_delivery

    def publish(self, delivery: Delivery, msg: Message, address: str):
        queue = self._queue(address)
        queue.publish(msg)
        unsettled_deliveries[delivery] = UnsettledDelivery(queue, msg)

    def _declare_txn(self):
        tid = bytes(str(uuid.uuid4()), 'UTF8')
        self.txns[tid] = []
        return tid

    def _discharge_txn(self, tid, failed):
        if not failed:
            # Commit
            print(f"Commit: txn-id={tid}")
            for action in self.txns[tid]:
                action.commit(self)
        else:
            # Rollback
            print(f"Rollback: txn-id={tid}")
            for action in self.txns[tid]:
                action.rollback(self)
        del self.txns[tid]

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
                    self._discharge_txn(tid, failed)
                    delivery.update(Disposition.ACCEPTED)
                else:
                    print(f"Discharge unknown txn-id: txn-id={tid}, failed={failed}")
                    delivery.local.condition = Condition('amqp:transaction:unknown-id')
                    delivery.update(Disposition.REJECTED)
        delivery.settle()

    def on_link_closing(self, event):
        if event.link.is_sender:
            self._unsubscribe(event.link)

    def _remove_stale_consumers(self, connection):
        link = connection.link_head(Endpoint.REMOTE_ACTIVE)
        while link:
            if link.is_sender:
                self._unsubscribe(link)
            link = link.next(Endpoint.REMOTE_ACTIVE)

    def on_connection_closing(self, event):
        self._remove_stale_consumers(event.connection)

    def on_disconnected(self, event):
        self._remove_stale_consumers(event.connection)

    def on_sendable(self, event):
        link: Link = event.link
        self._queue(link.source.address).dispatch(link)

    def on_message(self, event):
        link: Link = event.link
        delivery: Delivery = event.delivery

        msg = event.message
        if link.target.type == Terminus.COORDINATOR:
            # Deal with special transaction messages
            self._coordinator_message(msg, delivery)
            return

        address = link.target.address
        if address is None:
            address = msg.address

        if address is None:
            print("Message without address")
            delivery.local.condition = Condition('amqp:link:invalid-address')
            delivery.update(Disposition.REJECTED)
            delivery.settle()
            return

        # Is this a transactioned message?
        disposition = delivery.remote
        if disposition and disposition.type == Disposition.TRANSACTIONAL_STATE:
            tid = disposition.id
            if tid in self.txns:
                print(f"Message: txn-id={tid}")
                self.txns[tid].append(QueueMessage(delivery, msg, address))
                return
            else:
                print(f"Message unknown txn-id: txn-id={tid}")
                delivery.local.condition = Condition('amqp:transaction:unknown-id')
                delivery.update(Disposition.REJECTED)
                delivery.settle()
                return

        self.publish(delivery, msg, address)
        delivery.update(Disposition.ACCEPTED)
        delivery.settle()

    def on_delivery_updated(self, event):
        """Handle all delivery updates for the link."""
        delivery = event.delivery
        disposition = delivery.remote
        # Is this a transactioned delivery update?
        if disposition.type == Disposition.TRANSACTIONAL_STATE:
            tid = disposition.id
            outcome = disposition.outcome_type
            if tid in self.txns:
                print(f"Delivery update: txn-id={tid} outcome={outcome}")
                self.txns[tid].append(DeliveryUpdate(delivery, outcome))
                return
            else:
                print(f"Message unknown txn-id: txn-id={tid}")
                delivery.local.condition = Condition('amqp:transaction:unknown-id')
                delivery.update(Disposition.REJECTED)
        else:
            self.delivery_update(delivery, disposition.type)
        # The delivery is settled in every case except a valid transaction
        # where the outcome is not yet known until the transaction is discharged.
        delivery.settle()


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
