from _cffi_backend import FFI
from . import lib as lib

ffi = FFI()

__all__ = ["ffi", "lib"]
