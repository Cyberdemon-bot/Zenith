"""Runtime value representations for the Zenith prototype interpreter."""

from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional, Tuple
from signals import ZenithRuntimeError


class ZenithNullType:
    """Singleton representing Zenith `null`."""
    _instance: Optional["ZenithNullType"] = None

    def __new__(cls) -> "ZenithNullType":
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def __repr__(self) -> str:
        return "null"

    def __eq__(self, other: object) -> bool:
        return isinstance(other, ZenithNullType)


NULL = ZenithNullType()


@dataclass
class ZenithTuple:
    """Represents a Zenith tuple `(a, b)` or the unit value `()`."""
    elements: List[Any] = field(default_factory=list)
    is_mut: bool = False

    @property
    def is_unit(self) -> bool:
        return len(self.elements) == 0

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, ZenithTuple):
            return False
        return self.elements == other.elements

    def __repr__(self) -> str:
        if not self.elements:
            return "()"
        if len(self.elements) == 1:
            return f"({format_value(self.elements[0])},)"
        inner = ", ".join(format_value(e) for e in self.elements)
        return f"({inner})"


# Canonical immutable Unit instance
UNIT = ZenithTuple([], is_mut=False)


@dataclass
class ZenithStructDef:
    """Descriptor for a declared `struct` type."""
    name: str
    field_names: List[str]
    is_unit: bool = False


@dataclass
class ZenithStructInstance:
    """Runtime instance of a Zenith struct."""
    struct_name: str
    fields: Dict[str, Any]
    is_mut: bool = False

    def get_field(self, name: str) -> Any:
        if name not in self.fields:
            raise ZenithRuntimeError(f"Struct '{self.struct_name}' has no field '{name}'")
        return self.fields[name]

    def set_field(self, name: str, value: Any) -> None:
        if not self.is_mut:
            raise ZenithRuntimeError(
                f"Cannot mutate field '{name}' on immutable struct instance of '{self.struct_name}'"
            )
        if name not in self.fields:
            raise ZenithRuntimeError(f"Struct '{self.struct_name}' has no field '{name}'")
        self.fields[name] = value

    def clone(self, is_mut: bool = False) -> "ZenithStructInstance":
        return ZenithStructInstance(
            struct_name=self.struct_name,
            fields= {k: clone_value(v, is_mut) for k, v in self.fields.items()},
            is_mut=is_mut,
        )

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, ZenithStructInstance):
            return False
        return self.struct_name == other.struct_name and self.fields == other.fields

    def __repr__(self) -> str:
        if not self.fields:
            return f"{self.struct_name} {{}}"
        body = ", ".join(f"{k}: {format_value(v)}" for k, v in self.fields.items())
        return f"{self.struct_name} {{ {body} }}"


@dataclass
class ZenithRef:
    """Reference (`&x`, `mut &x`, or heap `new` handle) supporting deref read/write."""
    getter: Callable[[], Any]
    setter: Optional[Callable[[Any], None]]
    is_mut: bool = False
    label: str = "ref"

    def deref(self) -> Any:
        return self.getter()

    def assign_deref(self, value: Any) -> None:
        if not self.is_mut or self.setter is None:
            raise ZenithRuntimeError("Cannot assign through an immutable reference")
        self.setter(value)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, ZenithRef):
            return False
        return self.deref() == other.deref()

    def __repr__(self) -> str:
        prefix = "mut &" if self.is_mut else "&"
        return f"{prefix}{format_value(self.deref())}"


@dataclass
class ZenithFunction:
    """User-defined Zenith function."""
    name: str
    params: List[Tuple[str, bool]]  # (param_name, is_mut_ref)
    body: Any                       # BlockExpr AST node
    closure: Any                    # Defining Environment

    def __repr__(self) -> str:
        return f"<func {self.name}>"


def clone_value(val: Any, is_mut: bool = False) -> Any:
    """Adjusts mutability metadata when binding a value to a variable."""
    if isinstance(val, ZenithStructInstance):
        return ZenithStructInstance(
            struct_name=val.struct_name,
            fields=dict(val.fields),
            is_mut=is_mut,
        )
    if isinstance(val, ZenithTuple):
        if val.is_unit:
            return UNIT
        return ZenithTuple(list(val.elements), is_mut=is_mut)
    return val


def is_truthy(val: Any) -> bool:
    """Evaluates truthiness in conditions (`if`, `for`, `and`, `or`, `!`)."""
    if isinstance(val, bool):
        return val
    if isinstance(val, ZenithNullType):
        return False
    if isinstance(val, ZenithTuple) and val.is_unit:
        return False
    if isinstance(val, (int, float)):
        return val != 0
    return True


def format_value(val: Any) -> str:
    """Formats any Zenith runtime value for output via `print`."""
    if isinstance(val, bool):
        return "true" if val else "false"
    if isinstance(val, ZenithNullType):
        return "null"
    if isinstance(val, (ZenithTuple, ZenithStructInstance, ZenithRef, ZenithFunction)):
        return repr(val)
    return str(val)
