# Expressionist -- ctypes wrapper around the expressionist_c shared library.
#
# Copyright 2026 Paolo Bosetti
# SPDX-License-Identifier: Apache-2.0

import ctypes
import ctypes.util
import json
import os
import sys

__all__ = ["Expressionist", "EvalMethod", "ExpressionistError"]


class EvalMethod:
    GRAPH = 0
    RECURSIVE = 1


class ExpressionistError(Exception):
    pass


def _candidate_names():
    if sys.platform.startswith("win"):
        return ["expressionist_c.dll"]
    if sys.platform == "darwin":
        return ["libexpressionist_c.dylib"]
    return ["libexpressionist_c.so"]


def _load_library():
    override = os.environ.get("EXPRESSIONIST_C_LIBRARY")
    if override:
        return ctypes.CDLL(override)

    here = os.path.dirname(os.path.abspath(__file__))
    for name in _candidate_names():
        path = os.path.join(here, name)
        if os.path.exists(path):
            return ctypes.CDLL(path)

    found = ctypes.util.find_library("expressionist_c")
    if found:
        return ctypes.CDLL(found)

    raise OSError(
        "could not locate the expressionist_c shared library; set "
        "EXPRESSIONIST_C_LIBRARY to its path"
    )


_lib = _load_library()

_lib.expressionist_create.restype = ctypes.c_void_p
_lib.expressionist_create.argtypes = []

_lib.expressionist_destroy.restype = None
_lib.expressionist_destroy.argtypes = [ctypes.c_void_p]

_lib.expressionist_set_tag.restype = None
_lib.expressionist_set_tag.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.expressionist_set_disable_key.restype = None
_lib.expressionist_set_disable_key.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.expressionist_set_eval_method.restype = None
_lib.expressionist_set_eval_method.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.expressionist_evaluate.restype = ctypes.c_int
_lib.expressionist_evaluate.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_void_p),
    ctypes.POINTER(ctypes.c_void_p),
]

_lib.expressionist_free_string.restype = None
_lib.expressionist_free_string.argtypes = [ctypes.c_void_p]

_lib.expressionist_version.restype = ctypes.c_char_p
_lib.expressionist_version.argtypes = []


def _take_cstr(voidp):
    """Copy the C string out of a c_void_p and free the native buffer."""
    text = ctypes.cast(voidp, ctypes.c_char_p).value
    _lib.expressionist_free_string(voidp)
    return text.decode("utf-8")


class Expressionist:
    """Evaluates $-tagged expressions embedded in JSON, via the C ABI."""

    def __init__(self, method=EvalMethod.RECURSIVE):
        handle = _lib.expressionist_create()
        if not handle:
            raise MemoryError("failed to create an Expressionist handle")
        self._handle = handle
        self.set_eval_method(method)

    def close(self):
        if getattr(self, "_handle", None):
            _lib.expressionist_destroy(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def __del__(self):
        self.close()

    def set_tag(self, tag):
        _lib.expressionist_set_tag(self._handle, tag.encode("utf-8"))

    def set_disable_key(self, key):
        _lib.expressionist_set_disable_key(self._handle, key.encode("utf-8"))

    def set_eval_method(self, method):
        _lib.expressionist_set_eval_method(self._handle, int(method))

    def evaluate(self, data):
        """Evaluate `data` (a JSON-serializable object, or a JSON string)
        and return a new, evaluated object. `data` itself is never mutated.
        """
        json_in = data if isinstance(data, str) else json.dumps(data)

        out_json = ctypes.c_void_p()
        out_error = ctypes.c_void_p()
        rc = _lib.expressionist_evaluate(
            self._handle,
            json_in.encode("utf-8"),
            ctypes.byref(out_json),
            ctypes.byref(out_error),
        )
        if rc != 0:
            raise ExpressionistError(_take_cstr(out_error))
        return json.loads(_take_cstr(out_json))


def version():
    return _lib.expressionist_version().decode("utf-8")
