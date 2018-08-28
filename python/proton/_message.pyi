from typing import (
    Any,
    Optional,
    Union
)

from uuid import UUID

from proton._data import symbol, ulong
from proton._delivery import Delivery
from proton._endpoints import Sender


class Message:
    address: Optional[str]
    content_encoding: symbol
    content_type: symbol
    correlation_id: Optional[Union[ulong, UUID, str]]
    creation_time: float
    delivery_count: int
    durable: bool
    expiry_time: float
    first_acquirer: bool
    group_id: Optional[str]
    group_sequence: int
    id: Optional[Union[ulong, UUID, str]]
    inferred: bool
    priority: int
    reply_to: Optional[str]
    reply_to_group_id: Optional[str]
    subject: Optional[str]
    ttl: float
    user_id: bytes

    def __del__(self) -> None: ...
    def __init__(self, body: Optional[str] = None, **kwargs: Any) -> None: ...
    def _check(self, err: int) -> int: ...
    def _check_property_keys(self) -> None: ...
    def _post_decode(self) -> None: ...
    def _pre_encode(self) -> None: ...
    def decode(self, data: bytes) -> None: ...
    def encode(self) -> bytes: ...
    def send(self, sender: Sender, tag: None = None) -> Delivery: ...
