from typing import (
    Any,
    Dict,
    Iterable,
    NoReturn
)

def raise_(t: Any, v: Any=None, tb: Any=None) -> NoReturn: ...
def iteritems(d: Any, **kw: Dict[Any, Any]) -> Iterable: ...
