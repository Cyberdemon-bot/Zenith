"""Lexical environment and variable binding management with mutability enforcement."""

from dataclasses import dataclass
from typing import Any, Dict, Optional
from signals import ZenithRuntimeError
from values import ZenithRef, clone_value


@dataclass
class Binding:
    """Stores a variable's runtime value and whether the binding is mutable."""
    name: str
    value: Any
    is_mut: bool = False


class Environment:
    """Lexically scoped variable environment."""

    def __init__(self, parent: Optional["Environment"] = None):
        self.parent = parent
        self.bindings: Dict[str, Binding] = {}

    def define(self, name: str, value: Any, is_mut: bool = False) -> None:
        """Defines a new variable in the current lexical scope."""
        bound_val = clone_value(value, is_mut=is_mut)
        self.bindings[name] = Binding(name=name, value=bound_val, is_mut=is_mut)

    def resolve_binding(self, name: str) -> Binding:
        """Looks up the `Binding` for `name` across the scope chain."""
        if name in self.bindings:
            return self.bindings[name]
        if self.parent is not None:
            return self.parent.resolve_binding(name)
        raise ZenithRuntimeError(f"Undefined identifier '{name}'")

    def get(self, name: str) -> Any:
        """Reads the value of variable `name`."""
        return self.resolve_binding(name).value

    def assign(self, name: str, value: Any) -> None:
        """Assigns a new value to an existing variable if it was declared `let mut`."""
        binding = self.resolve_binding(name)
        if not binding.is_mut:
            raise ZenithRuntimeError(f"Cannot assign to immutable variable '{name}'")
        binding.value = clone_value(value, is_mut=True)

    def make_ref(self, name: str, is_mut: bool) -> ZenithRef:
        """Creates a reference (`&name` or `mut &name`) to an existing variable binding."""
        binding = self.resolve_binding(name)
        if is_mut and not binding.is_mut:
            raise ZenithRuntimeError(
                f"Cannot take mutable reference 'mut &{name}' to immutable variable '{name}'"
            )

        def getter() -> Any:
            return binding.value

        def setter(new_val: Any) -> None:
            if not binding.is_mut:
                raise ZenithRuntimeError(f"Cannot mutate immutable variable '{name}'")
            binding.value = clone_value(new_val, is_mut=True)

        return ZenithRef(getter=getter, setter=setter if is_mut else None, is_mut=is_mut, label=name)
