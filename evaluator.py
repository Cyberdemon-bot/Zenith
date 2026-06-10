class Environment:
    def __init__(self, outer=None) -> None:
        self.store: dict = {}
        self.outer = outer

    def get(self, name: str):
        # 1. Tìm kiếm trong không gian bộ nhớ hiện tại
        if name in self.store:
            return self.store[name]
        
        # 2. Nếu không thấy, leo ngược chuỗi scope cha lên toàn cục
        if self.outer:
            if hasattr(self.outer, 'get'):
                return self.outer.get(name)
            elif isinstance(self.outer, dict) and name in self.outer:
                return self.outer[name]
                
        raise RuntimeError(f"Couldnt find any variable '{name}' in this scope")

    def set(self, name: str, value):
        # 1. Nếu biến đã có sẵn ở scope cục bộ -> Ghi đè cập nhật giá trị mới
        if name in self.store:
            self.store[name] = value
            return value
            
        # 2. Nếu biến có ở scope cha -> Đẩy lệnh cập nhật lên scope cha xử lý
        if self.outer:
            if hasattr(self.outer, 'has') and self.outer.has(name):
                return self.outer.set(name, value)
            elif hasattr(self.outer, 'store') and name in self.outer.store:
                return self.outer.set(name, value)
            elif isinstance(self.outer, dict) and name in self.outer:
                self.outer[name] = value
                return value
                
        # 3. Nếu biến hoàn toàn mới (hoặc biến toàn cục) -> Khởi tạo tại scope hiện tại
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

# --- ĐỐI TƯỢNG ĐẶC BIỆT ĐỂ ĐIỀU KHIỂN DÒNG CHẠY Ở RUNTIME ---
class ReturnValue:
    def __init__(self, value):
        self.value = value

class BreakSignal:
    pass

class ContinueSignal:
    pass

class FunctionObject:
    def __init__(self, parameters, body, env):
        self.parameters = parameters  # List các dict chứa parameter {"name": "...", "value_type": "..."}
        self.body = body              # Cấu trúc BlockStatement dạng dict JSON
        self.env = env                # Lưu lại Lexical Environment (Closure)


# --- BỘ THỰC THI CHÍNH (EVALUATOR TỪ JSON) ---
class Evaluator:
    def __init__(self):
        pass

    def eval(self, node, env: Environment):
        if node is None:
            return None

        # Node lúc này là một Python dict được parse từ JSON
        if isinstance(node, dict):
            # Duyệt qua từng loại Node dựa vào Key của JSON
            if "statements" in node:  # Khối Program hoặc BlockStatement gốc có dạng {"statements": [...]}
                return self.__eval_statements(node["statements"], env)
            
            # --- CÁC CÂU LỆNH (STATEMENTS) ---
            if "Let" in node:
                let_data = node["Let"]
                val = self.eval(let_data["value"], env)
                env.store[let_data["name"]] = val
                return None

            if "Assign" in node:
                assign_data = node["Assign"]
                ident_node = assign_data["ident"]
                rvalue = self.eval(assign_data["rvalue"], env)
                operator = assign_data["operator"]

                # Trường hợp gán vào một phần tử mảng: arr[i] = rvalue
                if isinstance(ident_node, dict) and "Index" in ident_node:
                    index_data = ident_node["Index"]
                    arr = self.eval(index_data["left"], env)
                    idx = self.eval(index_data["index"], env)
                    
                    if not isinstance(arr, list):
                        raise RuntimeError("Lỗi thực thi: Không thể truy cập chỉ mục của đối tượng không phải mảng.")
                    
                    current_val = arr[idx]
                    new_val = self.__calculate_assign_value(current_val, operator, rvalue)
                    arr[idx] = new_val
                    return None
                
                # Trường hợp gán vào biến thông thường: x = rvalue
                elif isinstance(ident_node, dict) and "Identifier" in ident_node:
                    var_name = ident_node["Identifier"]
                    current_val = env.get(var_name)
                    new_val = self.__calculate_assign_value(current_val, operator, rvalue)
                    env.set(var_name, new_val)
                    return None
                
                raise RuntimeError("Lỗi thực thi: Vế trái câu lệnh gán không hợp lệ.")

            if "Return" in node:
                ret_val = self.eval(node["Return"]["return_value"], env)
                return ReturnValue(ret_val)

            if "Function" in node:
                func_data = node["Function"]
                func_obj = FunctionObject(
                    parameters=func_data["parameters"],
                    body=func_data["body"],
                    env=env
                )
                env.store[func_data["name"]] = func_obj
                return None

            if "While" in node:
                while_data = node["While"]
                while True:
                    condition_val = self.eval(while_data["condition"], env)
                    if not condition_val:
                        break
                    
                    res = self.eval(while_data["body"], env)
                    if isinstance(res, ReturnValue): return res
                    if isinstance(res, BreakSignal): break
                    if isinstance(res, ContinueSignal): continue
                return None

            if "For" in node:
                for_data = node["For"]
                # Khởi tạo scope riêng cục bộ cho vòng lặp For
                for_env = Environment(outer=env)
                
                # Chạy câu lệnh khởi tạo (var_declaration)
                self.eval({"Let": for_data["var_declaration"]}, for_env)

                while True:
                    condition_val = self.eval(for_data["condition"], for_env)
                    if not condition_val:
                        break
                    
                    res = self.eval(for_data["body"], for_env)
                    if isinstance(res, ReturnValue): return res
                    if isinstance(res, BreakSignal): break
                    # Gặp ContinueSignal thì vẫn phải chạy câu lệnh bước nhảy (action) ở dưới
                    
                    # Chạy câu lệnh bước nhảy (ví dụ: i++)
                    self.eval(for_data["action"], for_env)
                return None

            if "Break" in node or node == "Break":
                return BreakSignal()

            if "Continue" in node or node == "Continue":
                return ContinueSignal()

            if "Expression" in node:
                return self.eval(node["Expression"], env)

            # --- CÁC BIỂU THỨC (EXPRESSIONS) ---
            if "Identifier" in node:
                return env.get(node["Identifier"])

            if "Integer" in node:
                return int(node["Integer"])

            if "Float" in node:
                return float(node["Float"])

            if "Boolean" in node:
                return bool(node["Boolean"])

            if "String" in node:
                return str(node["String"])

            if "Array" in node:
                array_data = node["Array"]
                # Khởi tạo mảng tĩnh dựa trên kích thước định sẵn, hoặc mảng động nếu có phần tử
                if len(array_data["elements"]) == 0:
                    return [0] * array_data["size"]
                else:
                    return [self.eval(el, env) for el in array_data["elements"]]

            if "Index" in node:
                index_data = node["Index"]
                left_val = self.eval(index_data["left"], env)
                idx_val = self.eval(index_data["index"], env)
                if not isinstance(left_val, list):
                    raise RuntimeError("Lỗi thực thi: Đối tượng không hỗ trợ truy cập index.")
                return left_val[idx_val]

            if "Prefix" in node:
                prefix_data = node["Prefix"]
                right_val = self.eval(prefix_data["right"], env)
                op = prefix_data["operator"]
                if op == "-": return -right_val
                if op == "+": return right_val
                if op == "!": return not right_val
                raise RuntimeError(f"Lỗi thực thi: Không hỗ trợ toán tử prefix '{op}'")

            if "Infix" in node:
                infix_data = node["Infix"]
                left_val = self.eval(infix_data["left"], env)
                right_val = self.eval(infix_data["right"], env)
                op = infix_data["operator"]
                return self.__eval_infix_expression(left_val, op, right_val)

            if "Postfix" in node:
                postfix_data = node["Postfix"]
                left_node = postfix_data["left"]
                op = postfix_data["operator"]

                if isinstance(left_node, dict) and "Identifier" in left_node:
                    var_name = left_node["Identifier"]
                    current_val = env.get(var_name)
                    if not isinstance(current_val, int):
                        raise RuntimeError("Lỗi thực thi: Toán tử hậu tố chỉ áp dụng cho số nguyên.")
                    
                    if op == "++":
                        env.set(var_name, current_val + 1)
                        return current_val
                    elif op == "--":
                        env.set(var_name, current_val - 1)
                        return current_val
                raise RuntimeError("Lỗi thực thi: Toán tử hậu tố phải đi kèm biến.")

            if "If" in node:
                if_data = node["If"]
                for branch in if_data["branches"]:
                    cond_val = self.eval(branch["condition"], env)
                    if cond_val:
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
        raise RuntimeError(f"Toán tử gán không hợp lệ: {op}")

    def __eval_infix_expression(self, left, op, right):
        if op == "+": return left + right
        if op == "-": return left - right
        if op == "*": return left * right
        if op == "/": 
            if isinstance(left, int) and isinstance(right, int):
                return left // right # Phép chia nguyên đồng bộ QuickSort
            return left / right
        if op == "%": return left % right
        if op == "^": return left ** right
        if op == "==": return left == right
        if op == "!=": return left != right
        if op == "<": return left < right
        if op == "<=": return left <= right
        if op == ">": return left > right
        if op == ">=": return left >= right
        raise RuntimeError(f"Toán tử infix không hỗ trợ: {op}")

    def __apply_function(self, func_obj, args):
        if not isinstance(func_obj, FunctionObject):
            raise RuntimeError("Lỗi thực thi: Đối tượng được gọi không phải hàm số.")

        if len(args) != len(func_obj.parameters):
            raise RuntimeError(f"Sai số lượng tham số. Kỳ vọng {len(func_obj.parameters)}, nhận được {len(args)}.")

        extended_env = Environment(outer=func_obj.env)
        for param, arg in zip(func_obj.parameters, args):
            extended_env.store[param["name"]] = arg

        evaluated_result = self.eval(func_obj.body, extended_env)
        if isinstance(evaluated_result, ReturnValue):
            return evaluated_result.value
        return evaluated_result