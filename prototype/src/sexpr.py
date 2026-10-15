"""S-expression tokenizer and parser for Zenith's `dump_tree_ast` format."""

import re
from dataclasses import dataclass, field
from typing import Any, Dict, List, Union
from signals import ZenithRuntimeError


@dataclass
class SNode:
    """Represents a parsed S-expression node `(Tag :attr val ... children ...)`."""
    tag: str
    value: Any = None                        # Primary literal / name atom if present
    attrs: Dict[str, Any] = field(default_factory=dict)  # Keyword attributes like :op, :mut
    children: List["SNode"] = field(default_factory=list)

    def __repr__(self) -> str:
        parts = [self.tag]
        if self.value is not None:
            parts.append(repr(self.value))
        for k, v in self.attrs.items():
            parts.append(f":{k}={v!r}")
        if self.children:
            parts.append(f"children={len(self.children)}")
        return f"SNode({' '.join(parts)})"


_TOKEN_RE = re.compile(
    r"""
    \s*
    (?:
        (?P<LPAREN>\()
      | (?P<RPAREN>\))
      | (?P<STRING>"(?:\\.|[^"\\])*")
      | (?P<KEYWORD>:[a-zA-Z_][a-zA-Z0-9_]*)
      | (?P<ATOM>[^\s()"]+)
    )
    """,
    re.VERBOSE | re.DOTALL,
)


def _unescape_string(raw: str) -> str:
    """Strips surrounding quotes and processes standard escape sequences."""
    inner = raw[1:-1]
    out: List[str] = []
    i = 0
    n = len(inner)
    while i < n:
        ch = inner[i]
        if ch == "\\" and i + 1 < n:
            nxt = inner[i + 1]
            match nxt:
                case "n": out.append("\n")
                case "t": out.append("\t")
                case "r": out.append("\r")
                case '"': out.append('"')
                case "\\": out.append("\\")
                case "0": out.append("\0")
                case _: out.append(nxt)
            i += 2
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def _parse_atom(atom_str: str) -> Union[bool, int, float, str]:
    if atom_str == "true":
        return True
    if atom_str == "false":
        return False
    try:
        return int(atom_str)
    except ValueError:
        pass
    try:
        return float(atom_str)
    except ValueError:
        pass
    return atom_str


def parse_sexpr(text: str) -> SNode:
    """Parses Zenith S-expression dump text into a root `SNode` tree."""
    tokens: List[tuple[str, Any]] = []
    pos = 0
    length = len(text)

    while pos < length:
        if text[pos].isspace():
            pos += 1
            continue
        m = _TOKEN_RE.match(text, pos)
        if not m:
            snippet = text[pos : pos + 30]
            raise ZenithRuntimeError(f"Failed to tokenize S-expression near: {snippet!r}")
        pos = m.end()
        kind = m.lastgroup
        val = m.group(kind)  # type: ignore[arg-type]
        if kind == "STRING":
            tokens.append(("STRING", _unescape_string(val)))
        elif kind == "KEYWORD":
            tokens.append(("KEYWORD", val[1:]))  # strip leading ':'
        elif kind == "ATOM":
            tokens.append(("ATOM", _parse_atom(val)))
        else:
            tokens.append((kind, val))  # LPAREN or RPAREN

    if not tokens:
        raise ZenithRuntimeError("Empty S-expression input")

    idx = 0

    def parse_node() -> SNode:
        nonlocal idx
        if idx >= len(tokens) or tokens[idx][0] != "LPAREN":
            raise ZenithRuntimeError(f"Expected '(' at token index {idx}")
        idx += 1  # consume '('

        if idx >= len(tokens) or tokens[idx][0] != "ATOM":
            raise ZenithRuntimeError(f"Expected node tag after '(' at token index {idx}")
        tag = str(tokens[idx][1])
        idx += 1

        node = SNode(tag=tag)

        # Parse inline literals, keyword attributes (:op, :mut), and child nodes
        while idx < len(tokens) and tokens[idx][0] != "RPAREN":
            tk_kind, tk_val = tokens[idx]
            if tk_kind == "KEYWORD":
                idx += 1
                if idx >= len(tokens):
                    raise ZenithRuntimeError(f"Missing value for attribute :{tk_val}")
                _, attr_val = tokens[idx]
                idx += 1
                node.attrs[tk_val] = attr_val
            elif tk_kind == "LPAREN":
                node.children.append(parse_node())
            elif tk_kind in ("STRING", "ATOM"):
                node.value = tk_val
                idx += 1
            else:
                raise ZenithRuntimeError(f"Unexpected token {tk_kind}: {tk_val}")

        if idx >= len(tokens) or tokens[idx][0] != "RPAREN":
            raise ZenithRuntimeError(f"Unclosed '(' for node '{tag}'")
        idx += 1  # consume ')'
        return node

    return parse_node()
