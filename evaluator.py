import operator

class Environment:
    def __init__(self, outer=None) -> None:
        self.store: dict = {}
        self.outer = outer

    def get(self, name: str):
        if name in self.store:
            return self.store[name]
        
        if self.outer:
            if hasattr(self.outer, 'get'):
                return self.outer.get(name)
            elif isinstance(self.outer, dict) and name in self.outer:
                return self.outer[name]
                
        raise RuntimeError(f"Runtime Error: Undefined variable '{name}' in this scope.")

    def set(self, name: str, value):
        if name in self.store:
            self.store[name] = value
            return value
            
        if self.outer:
            if hasattr(self.outer, 'has') and self.outer.has(name):
                return self.outer.set(name, value)
            elif hasattr(self.outer, 'store') and name in self.outer.store:
                return self.outer.set(name, value)
            elif isinstance(self.outer, dict) and name in self.outer:
                self.outer[name] = value
                return value
                
        self.store[name] = value
        return value

    def has(self, name: str) -> bool:
        if name in self.store:
            return True
        if self.outer:
            if hasattr(self.outer, 'has'):
                return self.outer.has(name)
            if hasattr(self.outer, 'store'):
                return name in self.outer.store
            if isinstance(self.outer, dict):
                return name in self.outer
        return False

    def __repr__(self):
        return str({k: v for k, v in self.store.items() if not hasattr(v, 'body')})


class ReturnValue:
    def __init__(self, value):
        self.value = value

class BreakSignal:
    pass

class ContinueSignal:
    pass

class FunctionObject:
    def __init__(self, parameters, body, env):
        self.parameters = parameters  
        self.body = body             
        self.env = env              

    def __repr__(self):
        return f"<FunctionObject params={[p['name'] for p in self.parameters]}>"


class Evaluator:
    def __init__(self):
        self.infix_ops = {
            "+": operator.add, "-": operator.sub, "*": operator.mul,
            "%": operator.mod, "^": operator.pow, "==": operator.eq,
            "!=": operator.ne, "<": operator.lt, "<=": operator.le,
            ">": operator.gt, ">=": operator.ge
        }

    def execute_program(self, ast_root_node) -> tuple:
        global_env = Environment()
        self.eval(ast_root_node, global_env)
        main_output = None
        if "main" in global_env.store:
            main_func = global_env.store["main"]
            main_output = self.__apply_function(main_func, [])
        global_memory_state = {
            k: v for k, v in global_env.store.items() 
            if not isinstance(v, FunctionObject)
        }

        return main_output, global_memory_state

    def eval(self, node, env: Environment):
        if node is None:
            return None

        if isinstance(node, dict):
            if "statements" in node:
                return self.__eval_statements(node["statements"], env)
            
            if "Let" in node:
                let_data = node["Let"]
                val = self.eval(let_data["value"], env)
                env.store[let_data["name"]] = val
                return None

            if "Assign" in node:
                assign_data = node["Assign"]
                ident_node = assign_data["ident"]
                rvalue = self.eval(assign_data["rvalue"], env)
                op = assign_data["operator"]

                if isinstance(ident_node, dict) and "Index" in ident_node:
                    idx_data = ident_node["Index"]
                    arr = self.eval(idx_data["left"], env)
                    idx = self.eval(idx_data["index"], env)
                    
                    if not isinstance(arr, list):
                        raise RuntimeError("Runtime Error: Cannot access index of a non-array object.")
                    
                    arr[idx] = self.__calculate_assign_value(arr[idx], op, rvalue)
                    return None
                
                elif isinstance(ident_node, dict) and "Identifier" in ident_node:
                    var_name = ident_node["Identifier"]
                    current_val = env.get(var_name)
                    env.set(var_name, self.__calculate_assign_value(current_val, op, rvalue))
                    return None
                
                raise RuntimeError("Runtime Error: Invalid left-hand side in assignment expression.")

            if "Return" in node:
                ret_val = self.eval(node["Return"]["return_value"], env)
                return ReturnValue(ret_val)

            if "Function" in node:
                func_data = node["Function"]
                env.store[func_data["name"]] = FunctionObject(
                    parameters=func_data["parameters"],
                    body=func_data["body"],
                    env=env
                )
                return None

            if "While" in node:
                while_data = node["While"]
                while self.eval(while_data["condition"], env):
                    res = self.eval(while_data["body"], env)
                    if isinstance(res, ReturnValue): return res
                    if isinstance(res, BreakSignal): break
                    if isinstance(res, ContinueSignal): continue
                return None

            if "For" in node:
                for_data = node["For"]
                for_env = Environment(outer=env)
                self.eval({"Let": for_data["var_declaration"]}, for_env)

                while self.eval(for_data["condition"], for_env):
                    res = self.eval(for_data["body"], for_env)
                    if isinstance(res, ReturnValue): return res
                    if isinstance(res, BreakSignal): break
                    self.eval(for_data["action"], for_env)
                return None

            if "Break" in node or node == "Break":
                return BreakSignal()

            if "Continue" in node or node == "Continue":
                return ContinueSignal()

            if "Expression" in node:
                return self.eval(node["Expression"], env)

            if "Identifier" in node:
                return env.get(node["Identifier"])

            if "Integer" in node: return int(node["Integer"])
            if "Float" in node: return float(node["Float"])
            if "Boolean" in node: return bool(node["Boolean"])
            if "String" in node: return str(node["String"])

            if "Array" in node:
                array_data = node["Array"]
                if len(array_data["elements"]) == 0:
                    return [0] * array_data["size"]
                return [self.eval(el, env) for el in array_data["elements"]]

            if "Index" in node:
                idx_data = node["Index"]
                left_val = self.eval(idx_data["left"], env)
                idx_val = self.eval(idx_data["index"], env)
                if not isinstance(left_val, list):
                    raise RuntimeError("Runtime Error: Object is not subscriptable.")
                return left_val[idx_val]

            if "Prefix" in node:
                prefix_data = node["Prefix"]
                right_val = self.eval(prefix_data["right"], env)
                op = prefix_data["operator"]
                if op == "-": return -right_val
                if op == "+": return right_val
                if op == "!": return not right_val
                raise RuntimeError(f"Runtime Error: Unsupported prefix operator '{op}'.")

            if "Infix" in node:
                infix_data = node["Infix"]
                left_val = self.eval(infix_data["left"], env)
                right_val = self.eval(infix_data["right"], env)
                return self.__eval_infix_expression(left_val, infix_data["operator"], right_val)

            if "Postfix" in node:
                postfix_data = node["Postfix"]
                left_node = postfix_data["left"]
                op = postfix_data["operator"]

                if isinstance(left_node, dict) and "Identifier" in left_node:
                    var_name = left_node["Identifier"]
                    current_val = env.get(var_name)
                    if not isinstance(current_val, int):
                        raise RuntimeError("Runtime Error: Postfix operators can only be applied to integers.")
                    
                    step = 1 if op == "++" else -1 if op == "--" else 0
                    if step == 0: raise RuntimeError(f"Runtime Error: Unknown postfix operator '{op}'.")
                    
                    env.set(var_name, current_val + step)
                    return current_val
                raise RuntimeError("Runtime Error: Postfix operator requires a valid variable target.")

            if "If" in node:
                if_data = node["If"]
                for branch in if_data["branches"]:
                    if self.eval(branch["condition"], env):
                        return self.eval(branch["consequence"], env)
                if if_data.get("alternative") is not None:
                    return self.eval(if_data["alternative"], env)
                return None

            if "Call" in node:
                call_data = node["Call"]
                func_obj = self.eval(call_data["function"], env)
                args = [self.eval(arg, env) for arg in call_data["arguments"]]
                return self.__apply_function(func_obj, args)

        return None

    def __eval_statements(self, statements, env):
        result = None
        for statement in statements:
            result = self.eval(statement, env)
            if isinstance(result, ReturnValue):
                return result.value
            if isinstance(result, (BreakSignal, ContinueSignal)):
                return result
        return result

    def __calculate_assign_value(self, current, op, rvalue):
        if op == "=": return rvalue
        if op == "+=": return current + rvalue
        if op == "-=": return current - rvalue
        if op == "*=": return current * rvalue
        if op == "/=": return current / rvalue
        if op == "%=": return current % rvalue
        raise RuntimeError(f"Runtime Error: Invalid assignment operator '{op}'.")

    def __eval_infix_expression(self, left, op, right):
        if op == "/":
            if isinstance(left, int) and isinstance(right, int):
                return left // right 
            return left / right
            
        handler = self.infix_ops.get(op)
        if handler is None:
            raise RuntimeError(f"Runtime Error: Unsupported infix operator '{op}'.")
        return handler(left, right)

    def __apply_function(self, func_obj, args):
        if not isinstance(func_obj, FunctionObject):
            raise RuntimeError("Runtime Error: Target object is not callable.")

        if len(args) != len(func_obj.parameters):
            raise RuntimeError(f"Runtime Error: Arity mismatch. Expected {len(func_obj.parameters)}, got {len(args)}.")

        extended_env = Environment(outer=func_obj.env)
        for param, arg in zip(func_obj.parameters, args):
            extended_env.store[param["name"]] = arg

        evaluated_result = self.eval(func_obj.body, extended_env)
        if isinstance(evaluated_result, ReturnValue):
            return evaluated_result.value
        return evaluated_result