from typing import (
    Dict,
    Optional,
)

from proton._data import symbol

from cproton import pn_condition_t

class Condition:
    def __init__(self,
        name: str,
        description: Optional[str] = None,
        info: Optional[Dict[symbol, str]] = None
    ) -> None: ...
    def __eq__(self, o: object) -> bool: ...
    def __repr__(self) -> str: ...

def obj2cond(obj: Optional[Condition], cond: pn_condition_t) -> None: ...
def cond2obj(cond: pn_condition_t) -> Optional[Condition]: ...
