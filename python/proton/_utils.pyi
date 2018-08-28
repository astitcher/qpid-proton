from typing import (
    Any,
    Callable,
    Union,
    Optional
)

from proton._delivery import Delivery
from proton._endpoints import Receiver, Sender, Terminus
from proton._events import Event
from proton._exceptions import ProtonException, ConnectionException, LinkException
from proton._message import Message
from proton._reactor import Container
from proton._transport import SSLDomain
from proton._url import Url

def _is_settled(delivery: Delivery) -> bool: ...


class SendException(ProtonException): ...


class LinkDetached(LinkException): ...


class ConnectionClosed(ConnectionException): ...


class AtomicCount:
    def __init__(self, start: int = 0, step: int = 1) -> None: ...
    def next(self) -> int: ...


class BlockingConnection:
    def __init__(
        self,
        url: Url,
        timeout: Optional[float] = None,
        container: Optional[Container] = None,
        ssl_domain: Optional[SSLDomain] = None,
        heartbeat: Optional[float] = None,
        **kwargs: Any
    ) -> None: ...
    def _is_closed(self) -> int: ...
    def close(self) -> None: ...
    def create_receiver(
        self,
        address: None,
        credit: Optional[int] = None,
        dynamic: bool = False,
        handler: Optional[SyncRequestResponse] = None,
        name: None = None,
        options: None = None
    ) -> BlockingReceiver: ...
    def create_sender(
        self,
        address: None,
        handler: None = None,
        name: None = None,
        options: None = None
    ) -> BlockingSender: ...
    def on_transport_closed(self, event: Event) -> None: ...
    def on_transport_head_closed(self, event: Event) -> None: ...
    def on_transport_tail_closed(self, event: Event) -> None: ...
    def run(self) -> None: ...
    def wait(self, condition: Callable, timeout: bool = False, msg: Optional[str] = None) -> None: ...


class BlockingLink:
    def __getattr__(self, name: str) -> Union[Callable, Terminus]: ...
    def __init__(
        self,
        connection: BlockingConnection,
        link: Union[Receiver, Sender]
    ) -> None: ...
    def _checkClosed(self) -> None: ...


class BlockingReceiver:
    def __init__(
        self,
        connection: BlockingConnection,
        receiver: Receiver,
        fetcher: None,
        credit: int = 1
    ) -> None: ...


class BlockingSender:
    def __init__(self, connection: BlockingConnection, sender: Sender) -> None: ...
    def send(
        self,
        msg: Message,
        timeout: bool = False,
        error_states: None = None
    ) -> Delivery: ...


class SyncRequestResponse:
    def __init__(self, connection: BlockingConnection, address: None = None) -> None: ...
    def call(self, request: Message) -> Message: ...
    def on_message(self, event: Event) -> None: ...
    @property
    def reply_to(self) -> str: ...
