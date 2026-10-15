"""Control-flow signals and runtime exceptions for the Zenith prototype interpreter."""

from typing import Any


class ZenithRuntimeError(Exception):
    """Raised when a runtime error occurs during Zenith execution."""
    pass


class ReturnSignal(Exception):
    """Unwinds the call stack when a `return` statement executes."""
    def __init__(self, value: Any):
        self.value = value


class BreakSignal(Exception):
    """Unwinds blocks/loops until the matching labeled expression is reached."""
    def __init__(self, label: str, value: Any):
        self.label = label
        self.value = value


class ContinueSignal(Exception):
    """Skips the rest of the current loop body iteration."""
    pass
