import operator


# ---------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------

class Environment:
    """Lexically-scoped variable store. outer is always another Environment."""

    def __init__(self, outer: "Environment | None" = None) -> None:
        self.store: dict = {}
        self.mutable: set = set()
        self.outer = outer

    def get(self, name: str):
        env = self
        while env is not None:
            if name in env.store:
                val = env.store[name]
                # Transparently dereference borrows on read
                if isinstance(val, BorrowRef):
                    return self.get(val.target_name)
                return val
            env = env.outer
        raise RuntimeError(f"Runtime Error: Undefined variable '{name}'.")

    def get_raw(self, name: str):
        """Return the raw stored value without dereferencing BorrowRef."""
        env = self
        while env is not None:
            if name in env.store:
                return env.store[name]
            env = env.outer
        raise RuntimeError(f"Runtime Error: Undefined variable '{name}'.")

    def set(self, name: str, value):
        """Walk up the chain and update the first scope that owns the name,
        enforcing mutability. If nobody owns it, create it in the current
        (innermost) scope. Writes through &mut borrows to the original variable."""
        env = self
        while env is not None:
            if name in env.store:
                raw = env.store[name]
                # Write-through for &mut borrows
                if isinstance(raw, BorrowRef):
                    if not raw.mutable_ref:
                        raise RuntimeError(
                            f"Runtime Error: Cannot assign through immutable borrow '&{name}'."
                        )
                    return self.set(raw.target_name, value)
                if name not in env.mutable:
                    raise RuntimeError(f"Runtime Error: Cannot assign to immutable variable '{name}'.")
                env.store[name] = value
                return value
            env = env.outer
        self.store[name] = value
        self.mutable.add(name)
        return value

    def define(self, name: str, value, mutable: bool = True):
        """Always create in the current scope (used by Let / function params)."""
        self.store[name] = value
        if mutable:
            self.mutable.add(name)
        else:
            self.mutable.discard(name)
        return value

    def is_mutable(self, name: str) -> bool:
        env = self
        while env is not None:
            if name in env.store:
                return name in env.mutable
            env = env.outer
        return True

    def __repr__(self):
        return str({k: v for k, v in self.store.items() if not isinstance(v, FunctionObject)})


# ---------------------------------------------------------------------------
# Runtime value wrappers / control-flow signals
# ---------------------------------------------------------------------------

class ReturnValue:
    __slots__ = ("value",)
    def __init__(self, value): self.value = value

class BreakSignal:
    __slots__ = ()

class ContinueSignal:
    __slots__ = ()

class FunctionObject:
    __slots__ = ("parameters", "body", "env")
    def __init__(self, parameters, body, env):
        self.parameters = parameters
        self.body       = body
        self.env        = env
    def __repr__(self):
        return f"<fn({', '.join(p['name'] for p in self.parameters)})>"

class BorrowRef:
    """
    Runtime representation of a borrow (& or &mut).
    Stored as the value of the borrowing variable in the environment.
    Reads transparently dereference to the target's current value.
    Writes through &mut update the original target variable.
    """
    __slots__ = ("target_name", "mutable_ref")
    def __init__(self, target_name: str, mutable_ref: bool):
        self.target_name = target_name
        self.mutable_ref = mutable_ref
    def __repr__(self):
        prefix = "&mut " if self.mutable_ref else "&"
        return f"{prefix}{self.target_name}"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _zero_for_type(value_type: str):
    """Type-appropriate default/fill value for a base type."""
    if value_type == "float": return 0.0
    if value_type == "str":   return ""
    if value_type == "bool":  return False
    return 0  # "int" and anything else


def _default_value(value_type: str, dims_exprs, env: "Environment"):
    """Default value for a `let` declaration with no initializer
    (and not nullable)."""
    fill = _zero_for_type(value_type)
    if dims_exprs:
        dims = [_eval_node(d, env) for d in dims_exprs]
        return _make_nd_array(dims, fill=fill)
    return fill


def _make_nd_array(dimensions: list, fill=0):
    """Build a nested list for an N-dimensional array."""
    if not dimensions:
        return fill
    if len(dimensions) == 1:
        return [fill] * dimensions[0]
    return [_make_nd_array(dimensions[1:], fill) for _ in range(dimensions[0])]


def _fill_nd_array(target: list, source):
    """
    Copy values from source into target in-place, leaving extra cells at
    their default fill value.  Works recursively for nested lists.
    source may be shallower or shorter than target — that's fine.
    """
    if not isinstance(source, list):
        return
    for i, val in enumerate(source):
        if i >= len(target):
            break
        if isinstance(val, list) and isinstance(target[i], list):
            _fill_nd_array(target[i], val)
        else:
            target[i] = val


def _root_identifier_name(node):
    """Walk down a chain of Index nodes to find the root variable name, if any.
    e.g. a[1][2] -> "a" """
    cur = node
    while isinstance(cur, dict) and "Index" in cur:
        cur = cur["Index"]["left"]
    if isinstance(cur, dict) and "Identifier" in cur:
        return cur["Identifier"]
    return None


def _resolve_index_chain(node: dict, env: "Environment"):
    """
    Walk a chain of Index nodes to find the innermost list and final index.
    Returns (list_ref, int_index) ready for assignment.
    e.g.  a[1][2]  →  (a[1], 2)
    """
    chain = []
    cur = node
    while isinstance(cur, dict) and "Index" in cur:
        chain.append(cur["Index"]["index"])
        cur = cur["Index"]["left"]

    arr = _eval_expr(cur, env)

    for index_expr in reversed(chain[1:]):
        idx = _eval_node(index_expr, env)
        if not isinstance(arr, list):
            raise RuntimeError("Runtime Error: Cannot index a non-array.")
        arr = arr[idx]

    final_idx = _eval_node(chain[0], env)
    if not isinstance(arr, list):
        raise RuntimeError("Runtime Error: Cannot index a non-array.")
    return arr, final_idx


# ---------------------------------------------------------------------------
# Core eval — iterative where possible, recursive only for expressions
# ---------------------------------------------------------------------------

def _eval_node(node, env: Environment):
    """
    Main dispatch. Statements are handled iteratively inside
    _eval_block_iterative; this function handles single nodes / expressions.
    """
    if node == "Break":
        return BreakSignal()

    if node == "Continue":
        return ContinueSignal()

    if node is None:
        return None

    if not isinstance(node, dict):
        return None

    # ---- Statements --------------------------------------------------------

    if "Let" in node:
        return _exec_let(node["Let"], env)

    if "Assign" in node:
        return _exec_assign(node["Assign"], env)

    if "Return" in node:
        val = _eval_node(node["Return"]["return_value"], env)
        return ReturnValue(val)

    if "Function" in node:
        fd = node["Function"]
        env.define(fd["name"], FunctionObject(fd["parameters"], fd["body"], env))
        return None

    if "While" in node:
        return _exec_while(node["While"], env)

    if "For" in node:
        return _exec_for(node["For"], env)

    if "Break" in node or node == "Break":
        return BreakSignal()

    if "Continue" in node or node == "Continue":
        return ContinueSignal()

    if "Block" in node:
        block_env = Environment(outer=env)
        return _eval_block_iterative(node["Block"]["statements"], block_env)

    # A raw block dict (consequence / body of if / while / for)
    if "statements" in node:
        return _eval_block_iterative(node["statements"], env)

    if "Expression" in node:
        return _eval_node(node["Expression"], env)

    # ---- Expressions -------------------------------------------------------
    return _eval_expr(node, env)


def _eval_expr(node, env: Environment):
    """Pure-expression evaluator (no statement forms)."""
    if node is None:
        return None
    if not isinstance(node, dict):
        return None

    if "Identifier" in node:
        return env.get(node["Identifier"])

    if "Integer"  in node: return int(node["Integer"])
    if "Float"    in node: return float(node["Float"])
    if "Boolean"  in node: return bool(node["Boolean"])
    if "String"   in node: return str(node["String"])
    if "Void"     in node: return None

    if "Default"  in node: return None

    if "Array" in node:
        return [_eval_node(el, env) for el in node["Array"]["elements"]]

    if "Index" in node:
        left = _eval_expr(node["Index"]["left"], env)
        idx  = _eval_node(node["Index"]["index"], env)
        if not isinstance(left, list):
            raise RuntimeError("Runtime Error: Object is not subscriptable.")
        return left[idx]

    if "Prefix" in node:
        return _eval_prefix(node["Prefix"], env)

    if "Infix" in node:
        return _eval_infix(node["Infix"], env)

    if "Postfix" in node:
        return _eval_postfix(node["Postfix"], env)

    if "If" in node:
        return _eval_if(node["If"], env)

    if "Call" in node:
        return _eval_call(node["Call"], env)

    # fall-through for statement-level nodes that appear as expressions
    return _eval_node(node, env)


# ---------------------------------------------------------------------------
# Iterative block executor — eliminates one call-frame per statement
# ---------------------------------------------------------------------------

def _eval_block_iterative(statements: list, env: Environment):
    """
    Execute a list of statements without recursing back into _eval_node
    for the next statement. Returns the last value, or a control-flow signal.
    ReturnValue is NOT unwrapped here — it bubbles up to _apply_function.
    """
    result = None
    for stmt in statements:
        result = _eval_node(stmt, env)
        if isinstance(result, (ReturnValue, BreakSignal, ContinueSignal)):
            return result
    return result


# ---------------------------------------------------------------------------
# Statement executors
# ---------------------------------------------------------------------------

def _exec_let(let_data: dict, env: Environment):
    value_node  = let_data.get("value")
    nullable    = let_data.get("nullable", False)
    mutable     = let_data.get("mutable", True)
    value_type  = let_data.get("value_type")
    dims_exprs  = let_data.get("array_size")
    reference   = let_data.get("reference", False)
    mutable_ref = let_data.get("mutable_ref", False)

    # --- Borrow semantics ---------------------------------------------------
    if reference:
        if value_node is None or not (isinstance(value_node, dict) and "Identifier" in value_node):
            raise RuntimeError(
                f"Runtime Error: Borrow '{let_data['name']}' must reference a named variable."
            )
        target_name = value_node["Identifier"]

        # Ensure target exists
        env.get(target_name)  # raises if undefined

        # &mut requires the target to be declared mut
        if mutable_ref and not env.is_mutable(target_name):
            raise RuntimeError(
                f"Runtime Error: Cannot borrow '{target_name}' as mutable — it is not declared mut."
            )

        # The borrow variable itself is immutable (you can't rebind a reference)
        env.define(let_data["name"], BorrowRef(target_name, mutable_ref), mutable=False)
        return None

    # --- Owned value --------------------------------------------------------
    if value_node is not None:
        val = _eval_node(value_node, env)
    elif nullable:
        # `let x: int?;` — no initializer, nullable -> null
        val = None
    else:
        # `let x: int;` / `let a: int[5];` — zero value
        val = _default_value(value_type, dims_exprs, env)

    if dims_exprs and val is not None:
        dims = [_eval_node(d, env) for d in dims_exprs]
        target = _make_nd_array(dims, fill=_zero_for_type(value_type))
        if isinstance(val, list):
            _fill_nd_array(target, val)
        val = target

    env.define(let_data["name"], val, mutable=mutable)
    return None


def _exec_assign(assign_data: dict, env: Environment):
    ident_node = assign_data["ident"]
    rvalue     = _eval_node(assign_data["rvalue"], env)
    op         = assign_data["operator"]

    if isinstance(ident_node, dict) and "Index" in ident_node:
        root_name = _root_identifier_name(ident_node)
        if root_name is not None and not env.is_mutable(root_name):
            raise RuntimeError(f"Runtime Error: Cannot mutate immutable variable '{root_name}'.")
        arr, idx = _resolve_index_chain(ident_node, env)
        arr[idx] = _calc_assign(arr[idx], op, rvalue)
        return None

    if isinstance(ident_node, dict) and "Identifier" in ident_node:
        var_name    = ident_node["Identifier"]
        current_val = env.get(var_name)
        env.set(var_name, _calc_assign(current_val, op, rvalue))  # raises if immutable
        return None

    raise RuntimeError("Runtime Error: Invalid left-hand side in assignment.")


def _exec_while(while_data: dict, env: Environment):
    while _eval_node(while_data["condition"], env):
        res = _eval_block_iterative(while_data["body"]["statements"], Environment(outer=env))
        if isinstance(res, ReturnValue): return res
        if isinstance(res, BreakSignal): break
    return None


def _exec_for(for_data: dict, env: Environment):
    for_env = Environment(outer=env)
    _exec_let(for_data["var_declaration"], for_env)

    while _eval_node(for_data["condition"], for_env):
        res = _eval_block_iterative(for_data["body"]["statements"], Environment(outer=for_env))
        if isinstance(res, ReturnValue): return res
        if isinstance(res, BreakSignal): break
        _eval_node(for_data["action"], for_env)
    return None


# ---------------------------------------------------------------------------
# Expression helpers
# ---------------------------------------------------------------------------

_INFIX_OPS = {
    "+":  operator.add,  "-":  operator.sub,  "*": operator.mul,
    "%":  operator.mod,  "**": operator.pow,
    "==": operator.eq,   "!=": operator.ne,
    "<":  operator.lt,   "<=": operator.le,
    ">":  operator.gt,   ">=": operator.ge,
    "&":  operator.and_,   # Bitwise AND
    "|":  operator.or_,    # Bitwise OR
    "^":  operator.xor,    # Bitwise XOR
    "<<": operator.lshift, # Bitwise Left Shift
    ">>": operator.rshift, # Bitwise Right Shift
}


def _eval_prefix(data: dict, env: Environment):
    op = data["operator"]

    # Borrow prefix operators — produce a BorrowRef at expression level.
    # Usage: pass &x or &mut x as a function argument.
    if op in ("&", "&mut"):
        right_node = data["right"]
        if not (isinstance(right_node, dict) and "Identifier" in right_node):
            raise RuntimeError("Runtime Error: Borrow operator requires a named variable.")
        target_name = right_node["Identifier"]
        mutable_ref = op == "&mut"
        env.get(target_name)  # raises if undefined
        if mutable_ref and not env.is_mutable(target_name):
            raise RuntimeError(
                f"Runtime Error: Cannot borrow '{target_name}' as mutable — it is not declared mut."
            )
        return BorrowRef(target_name, mutable_ref)

    right = _eval_node(data["right"], env)
    if op == "-": return -right
    if op == "+": return +right
    if op == "!": return not right
    if op == "~": return ~right
    raise RuntimeError(f"Runtime Error: Unknown prefix operator '{op}'.")


def _eval_infix(data: dict, env: Environment):
    op = data["operator"]

    # Short-circuit logical operators — evaluate right only when needed
    if op == "||":
        left = _eval_node(data["left"], env)
        return left or _eval_node(data["right"], env)
    if op == "&&":
        left = _eval_node(data["left"], env)
        return left and _eval_node(data["right"], env)

    left  = _eval_node(data["left"],  env)
    right = _eval_node(data["right"], env)

    if left is None or right is None:
        return None

    if op == "/":
        if isinstance(left, int) and isinstance(right, int):
            return left // right
        return left / right

    handler = _INFIX_OPS.get(op)
    if handler is None:
        raise RuntimeError(f"Runtime Error: Unknown infix operator '{op}'.")
    return handler(left, right)


def _eval_postfix(data: dict, env: Environment):
    left_node = data["left"]
    op        = data["operator"]

    if not (isinstance(left_node, dict) and "Identifier" in left_node):
        raise RuntimeError("Runtime Error: Postfix target must be a variable.")

    var_name    = left_node["Identifier"]
    current_val = env.get(var_name)  # dereferences BorrowRef transparently

    # --- Null assertion: x! ---
    # Asserts the variable is not null. Returns the unwrapped value,
    # or raises if the value IS null.
    if op == "!":
        return current_val is not None

    # --- Increment / Decrement: x++  x-- ---
    if not isinstance(current_val, int):
        raise RuntimeError("Runtime Error: Postfix ++ / -- require an integer.")
    step = 1 if op == "++" else -1 if op == "--" else None
    if step is None:
        raise RuntimeError(f"Runtime Error: Unknown postfix operator '{op}'.")
    env.set(var_name, current_val + step)  # raises if immutable; writes through &mut
    return current_val  # post-increment semantics: return OLD value


def _eval_if(data: dict, env: Environment):
    for branch in data["branches"]:
        if _eval_node(branch["condition"], env):
            branch_env = Environment(outer=env)
            return _eval_block_iterative(branch["consequence"]["statements"], branch_env)
    if data.get("alternative") is not None:
        alt_env = Environment(outer=env)
        return _eval_block_iterative(data["alternative"]["statements"], alt_env)
    return None


def _eval_call(data: dict, env: Environment):
    func_obj = _eval_node(data["function"], env)
    args     = [_eval_node(arg, env) for arg in data["arguments"]]
    return _apply_function(func_obj, args)


def _apply_function(func_obj, args):
    if not isinstance(func_obj, FunctionObject):
        raise RuntimeError("Runtime Error: Object is not callable.")
    if len(args) != len(func_obj.parameters):
        raise RuntimeError(
            f"Runtime Error: Arity mismatch — "
            f"expected {len(func_obj.parameters)}, got {len(args)}."
        )
    fn_env = Environment(outer=func_obj.env)
    for param, arg in zip(func_obj.parameters, args):
        p_mutable     = param.get("mutable", True)
        p_mutable_ref = param.get("mutable_ref", False)
        p_reference   = param.get("reference", False)

        # If the argument is a BorrowRef, bind it directly so the param
        # acts as an alias inside the function body.
        if isinstance(arg, BorrowRef):
            if p_mutable_ref and not arg.mutable_ref:
                raise RuntimeError(
                    f"Runtime Error: Cannot pass immutable borrow as &mut parameter '{param['name']}'."
                )
            fn_env.define(param["name"], arg, mutable=False)
        else:
            fn_env.define(param["name"], arg, mutable=p_mutable)

    result = _eval_block_iterative(func_obj.body["statements"], fn_env)
    if isinstance(result, ReturnValue):
        return result.value
    return result


def _calc_assign(current, op: str, rvalue):
    if op == "=":  return rvalue
    if current is None or rvalue is None:
        return None
    if op == "+=": return current + rvalue
    if op == "-=": return current - rvalue
    if op == "*=": return current * rvalue
    if op == "/=": return current // rvalue if isinstance(current, int) and isinstance(rvalue, int) else current / rvalue
    if op == "%=": return current % rvalue
    raise RuntimeError(f"Runtime Error: Unknown assignment operator '{op}'.")


# ---------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------

class Evaluator:
    """
    Thin public wrapper kept for API compatibility.
    All real work is done by module-level functions above.
    """

    def execute_program(self, ast_root_node) -> tuple:
        global_env = Environment()
        _eval_block_iterative(ast_root_node["statements"], global_env)

        main_output = None
        if "main" in global_env.store:
            main_output = _apply_function(global_env.store["main"], [])

        global_memory_state = {
            k: v for k, v in global_env.store.items()
            if not isinstance(v, FunctionObject)
        }
        return main_output, global_memory_state