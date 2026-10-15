"""Typed AST node definitions and SNode-to-AST builder for Zenith."""

from dataclasses import dataclass, field
from typing import Any, List, Optional, Tuple
from sexpr import SNode
from signals import ZenithRuntimeError


class ASTNode:
    """Base class for all Zenith AST nodes."""
    pass


@dataclass
class Literal(ASTNode):
    kind: str  # "int", "float", "string", "bool", "null"
    value: Any


@dataclass
class Identifier(ASTNode):
    name: str


@dataclass
class TupleLiteral(ASTNode):
    elements: List[ASTNode] = field(default_factory=list)


@dataclass
class ParenExpr(ASTNode):
    expr: ASTNode


@dataclass
class UnaryExpr(ASTNode):
    op: str
    operand: ASTNode


@dataclass
class BinaryExpr(ASTNode):
    op: str
    lhs: ASTNode
    rhs: ASTNode


@dataclass
class BorrowExpr(ASTNode):
    is_mut: bool
    operand: ASTNode


@dataclass
class DerefExpr(ASTNode):
    operand: ASTNode


@dataclass
class CallExpr(ASTNode):
    callee: ASTNode
    args: List[ASTNode]


@dataclass
class FieldAccess(ASTNode):
    target: ASTNode
    field_name: str


@dataclass
class IndexExpr(ASTNode):
    target: ASTNode
    index: ASTNode


@dataclass
class StructInit(ASTNode):
    struct_name: str
    fields: List[Tuple[str, ASTNode]]  # (field_name, value_expr)


@dataclass
class NewExpr(ASTNode):
    is_mut: bool
    type_name: str
    init_kind: str  # "args" (constructor call) or "fields" (struct literal style)
    arg_exprs: List[ASTNode] = field(default_factory=list)
    field_inits: List[Tuple[str, ASTNode]] = field(default_factory=list)


@dataclass
class BlockExpr(ASTNode):
    statements: List[ASTNode] = field(default_factory=list)


@dataclass
class LabeledExpr(ASTNode):
    label: str
    body: ASTNode


@dataclass
class IfExpr(ASTNode):
    cond: ASTNode
    then_branch: ASTNode
    else_branch: Optional[ASTNode] = None


@dataclass
class ForExpr(ASTNode):
    cond: Optional[ASTNode]
    body: BlockExpr


@dataclass
class ForInExpr(ASTNode):
    var_name: str
    iterable: ASTNode
    body: BlockExpr


@dataclass
class ExprStmt(ASTNode):
    expr: ASTNode


@dataclass
class LetStmt(ASTNode):
    name: str
    is_mut: bool
    value: ASTNode


@dataclass
class AssignStmt(ASTNode):
    target: ASTNode
    value: ASTNode


@dataclass
class ReturnStmt(ASTNode):
    value: Optional[ASTNode] = None


@dataclass
class BreakStmt(ASTNode):
    label: str
    value: Optional[ASTNode] = None


@dataclass
class ContinueStmt(ASTNode):
    pass


@dataclass
class Param:
    name: str
    is_mut_ref: bool = False


@dataclass
class FuncDecl(ASTNode):
    name: str
    params: List[Param]
    body: BlockExpr


@dataclass
class StructDecl(ASTNode):
    name: str
    fields: List[str]
    is_unit: bool = False


@dataclass
class Program(ASTNode):
    statements: List[ASTNode] = field(default_factory=list)


def _extract_ident_info(snode: SNode) -> Tuple[str, bool]:
    """Extracts `(ident_name, is_mut)` from an Identifier or TypedIdentifier SNode."""
    if snode.tag == "Identifier":
        return str(snode.value), False
    if snode.tag == "TypedIdentifier":
        is_mut = bool(snode.attrs.get("mut", False))
        ident_node = snode.children[0]
        # Also check if type annotation is a mutable RefType
        if len(snode.children) > 1 and snode.children[1].tag == "RefType":
            if bool(snode.children[1].attrs.get("mut", False)):
                is_mut = True
        return str(ident_node.value), is_mut
    raise ZenithRuntimeError(f"Expected Identifier or TypedIdentifier, got '{snode.tag}'")


def build_ast(snode: SNode) -> ASTNode:
    """Converts a raw `SNode` tree into typed `ASTNode` objects."""
    match snode.tag:
        case "Program":
            return Program(statements=[build_ast(c) for c in snode.children])

        case "Integer":
            return Literal(kind="int", value=int(snode.value))

        case "Decimal":
            return Literal(kind="float", value=float(snode.value))

        case "String":
            return Literal(kind="string", value=str(snode.value) if snode.value is not None else "")

        case "Bool":
            return Literal(kind="bool", value=bool(snode.value))

        case "Null":
            return Literal(kind="null", value=None)

        case "Identifier":
            return Identifier(name=str(snode.value))

        case "Tuple":
            return TupleLiteral(elements=[build_ast(c) for c in snode.children])

        case "ParenExpr":
            return ParenExpr(expr=build_ast(snode.children[0]))

        case "UnaryExpr":
            op = str(snode.attrs.get("op", ""))
            return UnaryExpr(op=op, operand=build_ast(snode.children[0]))

        case "BinaryExpr":
            op = str(snode.attrs.get("op", ""))
            return BinaryExpr(
                op=op,
                lhs=build_ast(snode.children[0]),
                rhs=build_ast(snode.children[1]),
            )

        case "BorrowExpr":
            is_mut = bool(snode.attrs.get("mut", False))
            return BorrowExpr(is_mut=is_mut, operand=build_ast(snode.children[0]))

        case "DerefExpr":
            return DerefExpr(operand=build_ast(snode.children[0]))

        case "CallExpr":
            callee = build_ast(snode.children[0])
            arg_list_node = snode.children[1]
            args = [build_ast(c) for c in arg_list_node.children]
            return CallExpr(callee=callee, args=args)

        case "FieldAccess":
            target = build_ast(snode.children[0])
            field_ident = snode.children[1]
            return FieldAccess(target=target, field_name=str(field_ident.value))

        case "IndexExpr":
            target = build_ast(snode.children[0])
            index = build_ast(snode.children[1])
            return IndexExpr(target=target, index=index)

        case "StructInit":
            name_node = snode.children[0]
            field_list_node = snode.children[1]
            fields: List[Tuple[str, ASTNode]] = []
            for f_node in field_list_node.children:
                f_name = str(f_node.children[0].value)
                f_val = build_ast(f_node.children[1])
                fields.append((f_name, f_val))
            return StructInit(struct_name=str(name_node.value), fields=fields)

        case "NewExpr":
            is_mut = bool(snode.attrs.get("mut", False))
            type_node = snode.children[0]
            type_name = str(type_node.value)
            init_node = snode.children[1]
            if init_node.tag == "ArgList":
                return NewExpr(
                    is_mut=is_mut,
                    type_name=type_name,
                    init_kind="args",
                    arg_exprs=[build_ast(c) for c in init_node.children],
                )
            elif init_node.tag == "FieldInitList":
                fields = []
                for f_node in init_node.children:
                    f_name = str(f_node.children[0].value)
                    f_val = build_ast(f_node.children[1])
                    fields.append((f_name, f_val))
                return NewExpr(
                    is_mut=is_mut,
                    type_name=type_name,
                    init_kind="fields",
                    field_inits=fields,
                )
            raise ZenithRuntimeError(f"Unexpected NewExpr init node '{init_node.tag}'")

        case "BlockExpr":
            return BlockExpr(statements=[build_ast(c) for c in snode.children])

        case "LabeledExpr":
            label_node = snode.children[0]
            body_node = build_ast(snode.children[1])
            return LabeledExpr(label=str(label_node.value), body=body_node)

        case "IfExpr":
            cond = build_ast(snode.children[0])
            then_branch = build_ast(snode.children[1])
            else_branch = build_ast(snode.children[2]) if len(snode.children) > 2 else None
            return IfExpr(cond=cond, then_branch=then_branch, else_branch=else_branch)

        case "ForExpr":
            if len(snode.children) == 1:
                body = build_ast(snode.children[0])
                assert isinstance(body, BlockExpr)
                return ForExpr(cond=None, body=body)
            else:
                cond = build_ast(snode.children[0])
                body = build_ast(snode.children[1])
                assert isinstance(body, BlockExpr)
                return ForExpr(cond=cond, body=body)

        case "ForInExpr":
            var_ident = snode.children[0]
            iterable = build_ast(snode.children[1])
            body = build_ast(snode.children[2])
            assert isinstance(body, BlockExpr)
            return ForInExpr(var_name=str(var_ident.value), iterable=iterable, body=body)

        case "ExprStmt":
            return ExprStmt(expr=build_ast(snode.children[0]))

        case "LetStmt":
            name, is_mut = _extract_ident_info(snode.children[0])
            val = build_ast(snode.children[1])
            return LetStmt(name=name, is_mut=is_mut, value=val)

        case "AssignStmt":
            return AssignStmt(
                target=build_ast(snode.children[0]),
                value=build_ast(snode.children[1]),
            )

        case "ReturnStmt":
            val = build_ast(snode.children[0]) if snode.children else None
            return ReturnStmt(value=val)

        case "BreakStmt":
            label_node = snode.children[0]
            val = build_ast(snode.children[1]) if len(snode.children) > 1 else None
            return BreakStmt(label=str(label_node.value), value=val)

        case "ContinueStmt":
            return ContinueStmt()

        case "FuncDecl":
            sign_node = snode.children[0]
            body_node = build_ast(snode.children[1])
            assert isinstance(body_node, BlockExpr)
            func_name = str(sign_node.children[0].value)
            param_list_node = sign_node.children[1]
            params: List[Param] = []
            for p_node in param_list_node.children:
                p_name, p_mut = _extract_ident_info(p_node)
                params.append(Param(name=p_name, is_mut_ref=p_mut))
            return FuncDecl(name=func_name, params=params, body=body_node)

        case "StructDecl":
            struct_name = str(snode.children[0].value)
            if len(snode.children) == 1:
                return StructDecl(name=struct_name, fields=[], is_unit=True)
            field_list_node = snode.children[1]
            field_names: List[str] = []
            for f_node in field_list_node.children:
                f_name, _ = _extract_ident_info(f_node)
                field_names.append(f_name)
            return StructDecl(name=struct_name, fields=field_names, is_unit=False)

        case _:
            raise ZenithRuntimeError(f"Unsupported AST SNode tag: '{snode.tag}'")
