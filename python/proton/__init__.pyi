from typing import (
    Any
)

import logging

class NullHandler(logging.Handler):
    def handle(self, record: Any) -> None: ...
    def emit(self, record: Any) -> None: ...
    def createLock(self) ->  None: ...
