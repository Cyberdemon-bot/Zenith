"""Tree-walking evaluator for the Zenith prototype interpreter."""

from typing import Any, Dict, List
import ast_nodes as ast
from environment import Environment
from signals import BreakSignal, ContinueSignal, ReturnSignal, ZenithRuntimeError
from values import (
    NULL,
    UNIT,
    ZenithFunction,
    ZenithRef,
    ZenithStructDef,
    ZenithStructInstance,
    ZenithTuple,
    clone_value,
    format_value,
    is_truthy,
)


class Evaluator:
    """Executes Zenith AST nodes directly."""

    def __init__(self) -> None:
        self.global_env = Environment()
        self.structs: Dict[str, ZenithStructDef] = {}

    def run_program(self, program: ast.Program) -> Any:
        """Executes a parsed Zenith `Program` AST."""
        # Pass 1: Hoist struct and function declarations
        executable_stmts: List[ast.ASTNode] = []
        for stmt in program.statements:
            if isinstance(stmt, ast.StructDecl):
                self._register_struct(stmt)
            elif isinstance(stmt, ast.FuncDecl):
                self._register_func(stmt, self.global_env)
            else:
                executable_stmts.append(stmt)

        # Pass 2: Run top-level executable statements
        last_val: Any = UNIT
        for stmt in executable_stmts:
            last_val = self.eval(stmt, self.global_env)

        # Pass 3: If no top-level executable statements ran and `main` is defined, invoke `main()`
        if not executable_stmts and "main" in self.global_env.bindings:
            main_fn = self.global_env.get("main")
            if isinstance(main_fn, ZenithFunction):
                last_val = self._invoke_function(main_fn, [])

        return last_val

    def _register_struct(self, node: ast.StructDecl) -> None:
        self.structs[node.name] = ZenithStructDef(
            name=node.name,
            field_names=list(node.fields),
            is_unit=node.is_unit,
        )

    def _register_func(self, node: ast.FuncDecl, env: Environment) -> None:
        fn = ZenithFunction(
            name=node.name,
            params=[(p.name, p.is_mut_ref) for p in node.params],
            body=node.body,
            closure=env,
        )
        env.define(node.name, fn, is_mut=False)

    def eval(self, node: ast.ASTNode, env: Environment) -> Any:
        """Evaluates a single AST node within `env`."""
        match node:
            # --- Literals & Identifiers ---
            case ast.Literal(kind="int", value=v):
                return int(v)
            case ast.Literal(kind="float", value=v):
                return float(v)
            case ast.Literal(kind="string", value=v):
                return str(v)
            case ast.Literal(kind="bool", value=v):
                return bool(v)
            case ast.Literal(kind="null"):
                return NULL

            case ast.Identifier(name=name):
                return env.get(name)

            case ast.TupleLiteral(elements=elems):
                if not elems:
                    return UNIT
                return ZenithTuple([self.eval(e, env) for e in elems], is_mut=False)

            case ast.ParenExpr(expr=inner):
                return self.eval(inner, env)

            # --- Unary & Binary Expressions ---
            case ast.UnaryExpr(op=op, operand=operand):
                val = self.eval(operand, env)
                if isinstance(val, ZenithRef):
                    val = val.deref()
                match op:
                    case "-":
                        if not isinstance(val, (int, float)) or isinstance(val, bool):
                            raise ZenithRuntimeError(f"Unary '-' requires numeric operand, got {format_value(val)}")
                        return -val
                    case "!":
                        return not is_truthy(val)
                    case _:
                        raise ZenithRuntimeError(f"Unknown unary operator '{op}'")

            case ast.BinaryExpr(op=op, lhs=lhs_node, rhs=rhs_node):
                # Short-circuit logical operators
                if op == "and":
                    left = self.eval(lhs_node, env)
                    if not is_truthy(left):
                        return False
                    return is_truthy(self.eval(rhs_node, env))
                if op == "or":
                    left = self.eval(lhs_node, env)
                    if is_truthy(left):
                        return True
                    return is_truthy(self.eval(rhs_node, env))

                left = self.eval(lhs_node, env)
                right = self.eval(rhs_node, env)
                return self._apply_binary_op(op, left, right)

            # --- References & Heap Allocation ---
            case ast.BorrowExpr(is_mut=is_mut, operand=operand):
                return self._eval_borrow(operand, is_mut, env)

            case ast.DerefExpr(operand=operand):
                ref_val = self.eval(operand, env)
                if not isinstance(ref_val, ZenithRef):
                    raise ZenithRuntimeError(f"Cannot dereference non-reference value: {format_value(ref_val)}")
                return ref_val.deref()

            case ast.StructInit(struct_name=s_name, fields=field_inits):
                return self._instantiate_struct_fields(s_name, field_inits, env, is_mut=False)

            case ast.NewExpr(
                is_mut=is_mut,
                type_name=t_name,
                init_kind=init_kind,
                arg_exprs=arg_exprs,
                field_inits=field_inits,
            ):
                if init_kind == "fields":
                    instance = self._instantiate_struct_fields(t_name, field_inits, env, is_mut=is_mut)
                else:
                    evaled_args = [self.eval(a, env) for a in arg_exprs]
                    instance = self._instantiate_struct_args(t_name, evaled_args, is_mut=is_mut)

                cell = [instance]

                def getter() -> Any:
                    return cell[0]

                def setter(new_v: Any) -> None:
                    if not is_mut:
                        raise ZenithRuntimeError(f"Cannot mutate immutable heap allocation 'new {t_name}'")
                    cell[0] = clone_value(new_v, is_mut=True)

                return ZenithRef(getter=getter, setter=setter if is_mut else None, is_mut=is_mut, label=f"new {t_name}")

            # --- Postfix Access & Calls ---
            case ast.FieldAccess(target=target_node, field_name=f_name):
                target_val = self.eval(target_node, env)
                if isinstance(target_val, ZenithRef):
                    target_val = target_val.deref()
                if isinstance(target_val, ZenithStructInstance):
                    return target_val.get_field(f_name)
                raise ZenithRuntimeError(f"Cannot access field '{f_name}' on {format_value(target_val)}")

            case ast.IndexExpr(target=target_node, index=index_node):
                target_val = self.eval(target_node, env)
                if isinstance(target_val, ZenithRef):
                    target_val = target_val.deref()
                idx_val = self.eval(index_node, env)
                if not isinstance(idx_val, int) or isinstance(idx_val, bool):
                    raise ZenithRuntimeError(f"Index must be an integer, got {format_value(idx_val)}")
                if isinstance(target_val, ZenithTuple):
                    if idx_val < 0 or idx_val >= len(target_val.elements):
                        raise ZenithRuntimeError(f"Tuple index {idx_val} out of bounds (len {len(target_val.elements)})")
                    return target_val.elements[idx_val]
                if isinstance(target_val, str):
                    if idx_val < 0 or idx_val >= len(target_val):
                        raise ZenithRuntimeError(f"String index {idx_val} out of bounds (len {len(target_val)})")
                    return target_val[idx_val]
                raise ZenithRuntimeError(f"Value is not indexable: {format_value(target_val)}")

            case ast.CallExpr(callee=callee_node, args=arg_nodes):
                return self._eval_call(callee_node, arg_nodes, env)

            # --- Blocks, Labels, Control Flow ---
            case ast.BlockExpr(statements=stmts):
                block_env = Environment(parent=env)
                for stmt in stmts:
                    if isinstance(stmt, ast.StructDecl):
                        self._register_struct(stmt)
                    elif isinstance(stmt, ast.FuncDecl):
                        self._register_func(stmt, block_env)
                    else:
                        self.eval(stmt, block_env)
                # Block fallthrough value is always Unit `()`
                return UNIT

            case ast.LabeledExpr(label=label, body=body_node):
                try:
                    return self.eval(body_node, env)
                except BreakSignal as sig:
                    if sig.label == label:
                        return sig.value
                    raise

            case ast.IfExpr(cond=cond_node, then_branch=then_node, else_branch=else_node):
                cond_val = self.eval(cond_node, env)
                if is_truthy(cond_val):
                    return self.eval(then_node, env)
                elif else_node is not None:
                    return self.eval(else_node, env)
                return UNIT

            case ast.ForExpr(cond=cond_node, body=body_node):
                while True:
                    if cond_node is not None:
                        cond_val = self.eval(cond_node, env)
                        if not is_truthy(cond_val):
                            break
                    try:
                        self.eval(body_node, env)
                    except ContinueSignal:
                        continue
                return UNIT

            case ast.ForInExpr(var_name=var_name, iterable=iter_node, body=body_node):
                iter_val = self.eval(iter_node, env)
                is_mut_iter = False
                if isinstance(iter_val, ZenithRef):
                    is_mut_iter = iter_val.is_mut
                    iter_val = iter_val.deref()

                if isinstance(iter_val, ZenithTuple):
                    items = iter_val.elements
                elif isinstance(iter_val, list):
                    items = iter_val
                elif isinstance(iter_val, str):
                    items = list(iter_val)
                else:
                    raise ZenithRuntimeError(f"Cannot iterate over {format_value(iter_val)}")

                for idx in range(len(items)):
                    loop_env = Environment(parent=env)
                    if is_mut_iter and isinstance(iter_val, ZenithTuple):
                        # Bind a mutable reference or synced variable to element
                        elem_val = items[idx]
                        loop_env.define(var_name, elem_val, is_mut=True)
                    else:
                        loop_env.define(var_name, items[idx], is_mut=False)
                    try:
                        self.eval(body_node, loop_env)
                    except ContinueSignal:
                        continue
                return UNIT

            # --- Statements & Declarations ---
            case ast.ExprStmt(expr=expr_node):
                self.eval(expr_node, env)
                return UNIT

            case ast.LetStmt(name=name, is_mut=is_mut, value=val_node):
                val = self.eval(val_node, env)
                env.define(name, val, is_mut=is_mut)
                return UNIT

            case ast.AssignStmt(target=target_node, value=val_node):
                rhs = self.eval(val_node, env)
                self._eval_assign(target_node, rhs, env)
                return UNIT

            case ast.ReturnStmt(value=val_node):
                ret_val = self.eval(val_node, env) if val_node is not None else UNIT
                raise ReturnSignal(ret_val)

            case ast.BreakStmt(label=label, value=val_node):
                brk_val = self.eval(val_node, env) if val_node is not None else UNIT
                raise BreakSignal(label=label, value=brk_val)

            case ast.ContinueStmt():
                raise ContinueSignal()

            case ast.FuncDecl():
                self._register_func(node, env)
                return UNIT

            case ast.StructDecl():
                self._register_struct(node)
                return UNIT

            case _:
                raise ZenithRuntimeError(f"Unhandled AST node: {type(node).__name__}")

    def _eval_call(self, callee_node: ast.ASTNode, arg_nodes: List[ast.ASTNode], env: Environment) -> Any:
        # Raw symbol detection for built-in functions
        if isinstance(callee_node, ast.Identifier):
            sym = callee_node.name
            if sym == "print":
                args = [self.eval(a, env) for a in arg_nodes]
                print(" ".join(format_value(a) for a in args))
                return UNIT
            if sym == "range":
                args = [self.eval(a, env) for a in arg_nodes]
                if not all(isinstance(x, int) and not isinstance(x, bool) for x in args):
                    raise ZenithRuntimeError("range() arguments must be integers")
                if len(args) == 1:
                    return ZenithTuple(list(range(args[0])))
                if len(args) == 2:
                    return ZenithTuple(list(range(args[0], args[1])))
                if len(args) == 3:
                    return ZenithTuple(list(range(args[0], args[1], args[2])))
                raise ZenithRuntimeError(f"range() expects 1 to 3 arguments, got {len(args)}")
            if sym == "len":
                if len(arg_nodes) != 1:
                    raise ZenithRuntimeError(f"len() expects 1 argument, got {len(arg_nodes)}")
                val = self.eval(arg_nodes[0], env)
                if isinstance(val, ZenithRef):
                    val = val.deref()
                if isinstance(val, ZenithTuple):
                    return len(val.elements)
                if isinstance(val, str):
                    return len(val)
                raise ZenithRuntimeError(f"len() not supported for {format_value(val)}")
            if sym == "assert":
                if not (1 <= len(arg_nodes) <= 2):
                    raise ZenithRuntimeError("assert() expects 1 or 2 arguments")
                cond = self.eval(arg_nodes[0], env)
                if not is_truthy(cond):
                    msg = format_value(self.eval(arg_nodes[1], env)) if len(arg_nodes) == 2 else "Assertion failed"
                    raise ZenithRuntimeError(f"AssertionError: {msg}")
                return UNIT

        callee = self.eval(callee_node, env)
        evaled_args = [self.eval(a, env) for a in arg_nodes]
        if isinstance(callee, ZenithFunction):
            return self._invoke_function(callee, evaled_args)
        raise ZenithRuntimeError(f"Object is not callable: {format_value(callee)}")

    def _invoke_function(self, fn: ZenithFunction, args: List[Any]) -> Any:
        if len(args) != len(fn.params):
            raise ZenithRuntimeError(
                f"Function '{fn.name}' expected {len(fn.params)} argument(s), got {len(args)}"
            )
        call_env = Environment(parent=fn.closure)
        for (param_name, is_mut_ref), arg_val in zip(fn.params, args):
            if is_mut_ref and isinstance(arg_val, ZenithRef) and not arg_val.is_mut:
                raise ZenithRuntimeError(
                    f"Parameter '{param_name}' in '{fn.name}' requires a mutable reference 'mut &'"
                )
            call_env.define(param_name, arg_val, is_mut=False)

        try:
            return self.eval(fn.body, call_env)
        except ReturnSignal as ret:
            return ret.value
        except BreakSignal as brk:
            raise ZenithRuntimeError(f"Uncaught break to label '{brk.label}' inside function '{fn.name}'")

    def _eval_borrow(self, operand: ast.ASTNode, is_mut: bool, env: Environment) -> ZenithRef:
        if isinstance(operand, ast.Identifier):
            return env.make_ref(operand.name, is_mut=is_mut)

        if isinstance(operand, ast.FieldAccess):
            target_val = self.eval(operand.target, env)
            if isinstance(target_val, ZenithRef):
                if is_mut and not target_val.is_mut:
                    raise ZenithRuntimeError("Cannot take mutable field reference through immutable reference")
                target_val = target_val.deref()
            if not isinstance(target_val, ZenithStructInstance):
                raise ZenithRuntimeError(f"Cannot borrow field '{operand.field_name}' on non-struct")
            if is_mut and not target_val.is_mut:
                raise ZenithRuntimeError(
                    f"Cannot take mutable reference to field '{operand.field_name}' of immutable struct"
                )
            f_name = operand.field_name

            def getter() -> Any:
                return target_val.get_field(f_name)

            def setter(v: Any) -> None:
                target_val.set_field(f_name, v)

            return ZenithRef(getter=getter, setter=setter if is_mut else None, is_mut=is_mut)

        # Fallback: borrow temporary rvalue
        val = self.eval(operand, env)
        cell = [clone_value(val, is_mut=is_mut)]

        def getter() -> Any:
            return cell[0]

        def setter(v: Any) -> None:
            cell[0] = v

        return ZenithRef(getter=getter, setter=setter if is_mut else None, is_mut=is_mut)

    def _eval_assign(self, target_node: ast.ASTNode, rhs: Any, env: Environment) -> None:
        match target_node:
            case ast.Identifier(name=name):
                env.assign(name, rhs)

            case ast.DerefExpr(operand=operand):
                ref_val = self.eval(operand, env)
                if not isinstance(ref_val, ZenithRef):
                    raise ZenithRuntimeError("Cannot dereference-assign to non-reference value")
                ref_val.assign_deref(rhs)

            case ast.FieldAccess(target=obj_node, field_name=f_name):
                obj = self.eval(obj_node, env)
                if isinstance(obj, ZenithRef):
                    if not obj.is_mut:
                        raise ZenithRuntimeError(f"Cannot mutate field '{f_name}' through immutable reference")
                    obj = obj.deref()
                if not isinstance(obj, ZenithStructInstance):
                    raise ZenithRuntimeError(f"Cannot assign field '{f_name}' on non-struct value")
                obj.set_field(f_name, rhs)

            case ast.IndexExpr(target=arr_node, index=idx_node):
                arr = self.eval(arr_node, env)
                if isinstance(arr, ZenithRef):
                    if not arr.is_mut:
                        raise ZenithRuntimeError("Cannot mutate element through immutable reference")
                    arr = arr.deref()
                idx = self.eval(idx_node, env)
                if not isinstance(idx, int) or isinstance(idx, bool):
                    raise ZenithRuntimeError(f"Index must be an integer, got {format_value(idx)}")
                if isinstance(arr, ZenithTuple):
                    if not arr.is_mut:
                        raise ZenithRuntimeError("Cannot mutate element of immutable tuple")
                    if idx < 0 or idx >= len(arr.elements):
                        raise ZenithRuntimeError(f"Tuple index {idx} out of bounds")
                    arr.elements[idx] = rhs
                else:
                    raise ZenithRuntimeError(f"Cannot index-assign on {format_value(arr)}")

            case _:
                raise ZenithRuntimeError("Invalid assignment target")

    def _instantiate_struct_fields(
        self,
        struct_name: str,
        field_inits: List[tuple[str, ast.ASTNode]],
        env: Environment,
        is_mut: bool,
    ) -> ZenithStructInstance:
        fields_dict: Dict[str, Any] = {}
        struct_def = self.structs.get(struct_name)

        for f_name, f_expr in field_inits:
            if struct_def is not None and not struct_def.is_unit and f_name not in struct_def.field_names:
                raise ZenithRuntimeError(f"Struct '{struct_name}' has no declared field '{f_name}'")
            fields_dict[f_name] = self.eval(f_expr, env)

        if struct_def is not None and not struct_def.is_unit:
            for req_field in struct_def.field_names:
                if req_field not in fields_dict:
                    fields_dict[req_field] = NULL

        return ZenithStructInstance(struct_name=struct_name, fields=fields_dict, is_mut=is_mut)

    def _instantiate_struct_args(
        self,
        struct_name: str,
        args: List[Any],
        is_mut: bool,
    ) -> ZenithStructInstance:
        struct_def = self.structs.get(struct_name)
        fields_dict: Dict[str, Any] = {}
        if struct_def is not None and struct_def.field_names:
            if len(args) > len(struct_def.field_names):
                raise ZenithRuntimeError(
                    f"Constructor for '{struct_name}' expected at most {len(struct_def.field_names)} args, got {len(args)}"
                )
            for idx, f_name in enumerate(struct_def.field_names):
                fields_dict[f_name] = args[idx] if idx < len(args) else NULL
        else:
            for idx, val in enumerate(args):
                fields_dict[f"_{idx}"] = val
        return ZenithStructInstance(struct_name=struct_name, fields=fields_dict, is_mut=is_mut)

    def _apply_binary_op(self, op: str, left: Any, right: Any) -> Any:
        if isinstance(left, ZenithRef):
            left = left.deref()
        if isinstance(right, ZenithRef):
            right = right.deref()

        match op:
            case "==":
                if type(left) is not type(right) and not (
                    isinstance(left, (int, float))
                    and isinstance(right, (int, float))
                    and not isinstance(left, bool)
                    and not isinstance(right, bool)
                ):
                    return False
                return left == right
            case "!=":
                return not self._apply_binary_op("==", left, right)
            case "+":
                if isinstance(left, str) or isinstance(right, str):
                    return format_value(left) + format_value(right)
                self._check_numeric(op, left, right)
                return left + right
            case "-":
                self._check_numeric(op, left, right)
                return left - right
            case "*":
                self._check_numeric(op, left, right)
                return left * right
            case "/":
                self._check_numeric(op, left, right)
                if right == 0:
                    raise ZenithRuntimeError("Division by zero")
                if isinstance(left, int) and isinstance(right, int):
                    # Integer truncation toward zero (C/Rust semantics)
                    return int(left / right)
                return left / right
            case "%":
                self._check_numeric(op, left, right)
                if right == 0:
                    raise ZenithRuntimeError("Modulo by zero")
                return left % right
            case "<":
                self._check_comparable(op, left, right)
                return left < right
            case "<=":
                self._check_comparable(op, left, right)
                return left <= right
            case ">":
                self._check_comparable(op, left, right)
                return left > right
            case ">=":
                self._check_comparable(op, left, right)
                return left >= right
            case _:
                raise ZenithRuntimeError(f"Unknown binary operator '{op}'")

    @staticmethod
    def _check_numeric(op: str, left: Any, right: Any) -> None:
        if (
            not isinstance(left, (int, float))
            or isinstance(left, bool)
            or not isinstance(right, (int, float))
            or isinstance(right, bool)
        ):
            raise ZenithRuntimeError(
                f"Operator '{op}' requires numeric operands, got {format_value(left)} and {format_value(right)}"
            )

    @staticmethod
    def _check_comparable(op: str, left: Any, right: Any) -> None:
        if isinstance(left, str) and isinstance(right, str):
            return
        Evaluator._check_numeric(op, left, right)
