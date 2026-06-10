// src/main.rs
pub mod token;
pub mod lexer;
pub mod ast;
mod parser;

use std::env;     // Thư viện đọc tham số dòng lệnh
use std::fs;      // Thư viện thao tác file (Đọc/Ghi)
use std::process; // Thư viện quản lý tiến trình (để exit khi có lỗi)

use lexer::Lexer;
use parser::Parser;
use token::TokenType; // Thêm vào để phục vụ việc check debug token

fn main() {
    // =========================================================================
    // ⚙️ BIẾN ĐIỀU HƯỚNG BREAK POINT (CHỈNH SỬA Ở ĐÂY)
    // - true: Chỉ chạy Lexer và in ra danh sách Token (Dùng để test Lexer).
    // - false: Chạy toàn bộ mạch Parser để sinh ra cây AST và file JSON.
    // =========================================================================
    let debug_lexer_only: bool = false; 

    // 1. Lấy danh sách các tham số truyền vào từ terminal
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        println!("❌ Lỗi: Thiếu đường dẫn file mã nguồn!");
        println!("💡 Hướng dẫn sử dụng: cargo run <đường_dẫn_file>");
        process::exit(1);
    }

    let file_path = &args[1];
    println!("--- ĐANG ĐỌC FILE MÃ NGUỒN: {} ---", file_path);

    // 2. Đọc toàn bộ nội dung file mã nguồn
    let file_content = match fs::read_to_string(file_path) {
        Ok(content) => content,
        Err(err) => {
            println!("❌ Không thể đọc file '{}': {}", file_path, err);
            process::exit(1);
        }
    };

    // =========================================================================
    // 🔍 XỬ LÝ BREAK POINT: KIỂM TRA RIÊNG LEXER
    // =========================================================================
    if debug_lexer_only {
        println!("--- BẮT ĐẦU PHÂN TÍCH TỪ VỰNG (LEXER ONLY DEBUG) ---");
        let mut debug_lexer = Lexer::new(&file_content);
        let mut token_count = 0;

        loop {
            let tok = debug_lexer.next_token();
            println!("Token [{}]: {:?}", token_count, tok);
            token_count += 1;

            if tok.token_type == TokenType::EOF {
                break;
            }
        }
        println!("--- HOÀN TẤT IN DANH SÁCH TOKEN (DỪNG CHƯƠNG TRÌNH) ---");
        return; // Thoát chương trình tại đây, không chạy Parser bên dưới
    }

    // =========================================================================
    // MẠCH CHẠY PARSER BÌNH THƯỜNG
    // =========================================================================
    println!("--- BẮT ĐẦU PHÂN TÍCH CÚ PHÁP (PARSING) ---");

    // 3. Khởi tạo Lexer và Parser (Dạng Zero-copy)
    let lexer = Lexer::new(&file_content);
    let mut parser = Parser::new(lexer);
    
    // 4. Tạo cây AST
    let program = parser.parse_program();

    // 5. Kiểm tra lỗi cú pháp
    if !parser.errors.is_empty() {
        println!("Phát hiện lỗi cú pháp trong file:");
        for err in parser.errors {
            println!("  ❌ {}", err);
        }
        process::exit(1);
    }

    // 6. Chuyển đổi cây AST sang định dạng chuỗi JSON đẹp đẽ (Pretty Print)
    let json_str = match serde_json::to_string_pretty(&program) {
        Ok(str_data) => str_data,
        Err(e) => {
            println!("❌ Không thể tạo chuỗi JSON: {}", e);
            process::exit(1);
        }
    };

    // 8. TIẾN HÀNH GHI FILE: Xuất toàn bộ chuỗi json_str ra file 'ast.json'
    let output_file = "ast.json";
    match fs::write(output_file, &json_str) {
        Ok(_) => {
            println!("--- GHI FILE THÀNH CÔNG ---");
            println!("💾 Đã lưu cây cú pháp AST tại: {}", output_file);
        }
        Err(err) => {
            println!("❌ Lỗi không thể ghi file '{}': {}", output_file, err);
            process::exit(1);
        }
    }

    println!("--- HOÀN TẤT CHƯƠNG TRÌNH ---");
}