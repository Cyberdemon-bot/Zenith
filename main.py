# main.py
import json
from evaluator import Evaluator, Environment

def main():
    # 1. Đọc cây cú pháp AST từ file JSON do bản Rust xuất ra
    with open("ast.json", "r", encoding="utf-8") as f:
        ast_data = json.load(f)

    # 2. Khởi tạo môi trường bộ nhớ toàn cục và bộ thực thi
    global_env = Environment()
    evaluator = Evaluator()

    print("--- BẮT ĐẦU CHẠY EVALUATOR TỪ FILE JSON ---")
    
    # 3. Nạp toàn bộ chương trình vào hệ thống
    evaluator.eval(ast_data, global_env)

    # 4. Tìm kiếm hàm main để kích hoạt chạy kết quả cuối cùng
    if "main" in global_env.store:
        main_func = global_env.store["main"]
        # Thực thi gọi hàm main() không đối số
        result = evaluator._Evaluator__apply_function(main_func, [])
        print(f"🎉 Kết quả thực thi hàm main(): {result}")
        print(f"👉 Giá trị gốc nhân 10 chia 1: {result} (Tương ứng trung vị thực tế là {result/10})")
    else:
        print("❌ Không tìm thấy hàm main trong file mã nguồn JSON!")

if __name__ == "__main__":
    main()