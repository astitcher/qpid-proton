from typing import (
    Dict,
    Optional,
    Union
)


class Url:
    scheme: Optional[str]
    username: Optional[str]
    password: Optional[str]
    host: Optional[str]
    port: Optional[int]
    path: Optional[str]
    params: Optional[str]
    query: Optional[str]
    fragment: Optional[str]

    def __init__(self, url: Optional[Union[Url, str]] = None, defaults: bool = True, **kwargs: Dict[str,str]) -> None: ...
    def defaults(self) -> Url: ...
