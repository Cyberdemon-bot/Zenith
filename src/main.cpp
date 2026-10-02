#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "Zenith/Compiler/Lexer.h"
#include "Zenith/Compiler/Parser.h"
#include "Zenith/Assembler/Assembler.h"
#include "Zenith/Core/VirtualMachine.h"


std::string read_file(const std::string& filepath)
{
    std::ifstream file(filepath);

    if (!file.is_open())
    {
        throw std::runtime_error(
            "Cannot open script file: " + filepath
        );
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

void dump_memory(const Zenith::Stack& stack, uint32_t register_count)
{
    std::cout << "\n--- R64 BANK DUMP ---\n";
    for (uint32_t i = 0; i < register_count; ++i)
    {
        uint64_t raw = stack.read<uint64_t>(i);
        int64_t i_val = std::bit_cast<int64_t>(raw);
        double d_val = std::bit_cast<double>(raw);
        std::cout
            << "R64[" << std::dec << std::setfill('0') << std::setw(2) << i << "] = HEX: 0x"
            << std::hex << std::setfill('0') << std::setw(16) << raw << std::dec << std::setfill(' ')
            << " | I64: " << std::setw(20) << i_val
            << " | F64: " << std::setw(22) << std::left << d_val << std::right
            << "\n";
    }
}


void dump_constants(const std::vector<uint64_t>& constants)
{
    std::cout << "\n--- CONSTANT POOL (" << constants.size() << " entries) ---\n";
    for (size_t i = 0; i < constants.size(); ++i)
    {
        uint64_t raw = constants[i];
        int64_t i_val = std::bit_cast<int64_t>(raw);
        double d_val = std::bit_cast<double>(raw);
        std::cout 
            << "K[" << std::dec << std::setfill('0') << std::setw(2) << i << "] = HEX: 0x"
            << std::hex << std::setfill('0') << std::setw(16) << raw << std::dec << std::setfill(' ')
            << " | I64: " << std::setw(20) << i_val
            << " | F64: " << std::setw(22) << std::left << d_val << std::right
            << "\n";
    }
}

void run_lexer_test()
{
    std::string_view source_code = R"(
    // Zenith Language Test Suite
    /* 
        Thử nghiệm Comment khối
        /* Comment khối lồng nhau (Nested block comment) */
    */

    pub struct Point {
        pub x: i32,
        pub y: i32,
    }

    pub class Player {
        pub name: String,
        pub hp: i32,
    }

    // 1. Hàm Native & Tham chiếu mượn (&mut T)
    pub native func move_point(pt: &mut Point, dx: i32, dy: i32) -> () {
        pt.x = pt.x + dx;
        pt.y = pt.y + dy;
    }

    // 2. Kiểu Result (T ! E) & Chuỗi Escape
    pub func load_player(id: i32) -> Player ! String {
        if id <= 0 => return Err("Invalid ID\n");

        let p = new Player();
        p.name = "Hero \"Zenith\"";
        p.hp = 100;
        return Ok(p);
    }

    // 3. Hàm Main & Các cú pháp nâng cao
    pub func main() -> () {
        // Variable & Literals
        let mut pt = Point { x: 10, y: 20 };
        let ratio = 3.14; // Decimal literal
        
        // Field access vs Decimal (pt.x)
        move_point(&mut pt, 5, 10);
        let x_val = pt.x;

        // Dual Memory Allocation
        let struct_buf = new Point[100];
        let class_refs = new &Player[10];

        // Concise Branching (Fat Arrow =>)
        let status = if pt.x > 10 => 200 else => 400;

        // Expression Block & Labeled Break ('calc)
        let score = 'calc: {
            if status == 400 {
                break 'calc 0;
            }
            let base = 50;
            break base * 2 + 10;
        };

        // Error Propagation Operator (?)
        let player = load_player(1)?;

        // Pattern Matching
        let msg = match load_player(2) {
            Ok(p) => {
                break "Success";
            },
            Err(e) => {
                break "Failed";
            }
        };
    }
    )";

    Zenith::Lexer lexer(source_code);
    
    std::cout << std::left 
              << std::setw(18) << "TOKEN TYPE" 
              << std::setw(25) << "LITERAL" 
              << std::setw(10) << "LINE" 
              << std::setw(10) << "COLUMN" 
              << "\n";
    std::cout << std::string(63, '-') << "\n";

    while (true)
    {
        Zenith::Token tok = lexer.next_token();
        std::string formatted_literal = "'" + std::string(tok.literal) + "'";

        std::cout << std::left 
                  << std::setw(18) << tok.type2string()
                  << std::setw(25) << formatted_literal
                  << std::setw(10) << tok.line 
                  << std::setw(10) << tok.column 
                  << "\n";
        if (tok.type == Zenith::TokenType::END_OF_FILE) break;
    }

    std::cout << std::string(63, '-') << "\n";
}

void run_zasm_test(const std::string& source)
{
    auto stack = std::make_unique<Zenith::Stack>();
    Zenith::State state{ .stack = stack.get() };
    Zenith::Machine vm;
    Zenith::Assembler assembler;
    Zenith::AssembledProgram program = assembler.assemble(source);
    state.proto = Zenith::Proto
    {
        .code = program.code,
        .numeric_constants = program.constants,
        .register_count = program.register_count
    };

    std::cout << "Code size       : " << state.proto.code.size() << "\n";
    std::cout << "Constant count  : " << state.proto.numeric_constants.size() << "\n";
    std::cout << "Register count  : " << state.proto.register_count << "\n";
    dump_constants(program.constants);
    std::cout << "\n[ BEFORE EXECUTION ]\n";
    dump_memory(*stack, state.proto.register_count);
    vm.execute(state);
    std::cout << "\n[ AFTER EXECUTION ]\n";
    dump_memory(*stack, state.proto.register_count);
}

void run_parser_expr_test() 
{
    std::string_view source_code = R"(
    'main_block: {

        // 1. LetStmt: Các dạng khai báo biến & Type Annotation
        let simple_int = 42;
        let decimal_val = 3.14159;
        let text_str = "Zenith Compiler";
        let typed_var: i32 = 100;
        let ref_mut_var: &mut String = &text_str; // Chuẩn Zenith: Borrow chỉ là &expr

        // 2. Prefix Expr: Unary minus (-), Bang (!), Grouping (), Borrow (&)
        let neg_expr = -simple_int;
        let not_expr = !simple_int;
        let grouped = (simple_int + 10);
        let borrowed_ref = &simple_int;

        // 3. Infix Expr & Precedence: Số học, Lũy thừa (^), So sánh
        let math_res = (simple_int + 5) * 2 / 4 % 3 ^ 2;
        let cmp_res = math_res >= 10 == (decimal_val < 5.0);

        // 4. Infix Expr: Field Access (.), Indexing ([]), Function Call (())
        let age = user.profile.age;
        let first_elem = list[0];
        let calc_res = compute(math_res, borrowed_ref, "literal_arg");

        // 5. Labeled Block Expression gán cho biến + Break 'label expr
        let search_res = 'search: {
            break 'search 100;
        };

        // 6. Control Flow Stmts: Continue, Break thuần, Break 'label
        'loop_label: {
            continue;
            break 'loop_label;
        };

        // 7. Expression Statement (Expr + ;)
        process_data(search_res);

        // 8. If-Else Expressions (Arrow, Block & Chain variations)
        // 8.1 Dạng Arrow đơn giản gán biến
        let if_arrow = if math_res >= 10 => 1 else => 0;

        // 8.2 Dạng Block đơn giản
        let if_block = if decimal_val < 5.0 { break 100; } else { break 200; };

        // 8.3 Kết hợp Arrow và Block trong chuỗi else if
        let if_chain = if simple_int < 0 => "negative" 
               else if simple_int == 0 { break "zero"; } 
               else => "positive";

        // 8.4 If không có else
        if typed_var > 50 => process_data(typed_var);

        // 8.5 If lồng bên trong biểu thức khác
        let nested_if = if (if simple_int > 0 => true else => false) { break 999; } else { break -1; };

        // 9. ReturnStmt
        return search_res;
    }
    )";
    std::cout << "================ PARSER TEST ================\n";
    
    Zenith::Parser parser(source_code);
    uint32_t root = parser.parse_program();

    std::cout << "Root Node Index: " << root << "\n";
    parser.dump_ast();

    if (parser.has_errors())
    {
        std::cout << "================ PARSER ERROR TEST ================\n";
        std::cout << "\n[Captured Parsing Errors (" << parser.get_errors_count() << ")]:\n";
        parser.dump_errors();
    }
}

void run_parser_error_test() 
{
    std::string_view source_code = R"(
    'error_test_block: {

        let = 42;
        let no_assignment 100;
        let bad_type: &mut = 10;
        let unclosed_paren = (10 + 20;
        let bad_field_access = user.;
        let bad_call = compute(arg1 arg2);
        let bad_index = list[0;
        let missing_semicolon = 50
        return 99
    }
    )";
    
    std::cout << "================ PARSER ERROR TEST ================\n";
    
    Zenith::Parser parser(source_code);
    uint32_t root = parser.parse_program();

    std::cout << "Root Node Index: " << root << "\n";

    if (parser.has_errors())
    {
        std::cout << "\n[Captured Parsing Errors (" << parser.get_errors_count() << ")]:\n";
        parser.dump_errors();
    }
    else
    {
        std::cout << "\nNo errors found (Unexpected!). AST:\n";
        parser.dump_ast();
    }
}



int main(int argc, char* argv[])
{
    // std::string filename = "";
    // if (argc > 1) filename = argv[1];
    // if (filename.empty()) return 0;
  
    // std::cout << "Reading script: " << filename << "...\n\n";
    // std::string source = read_file(filename);

    // run_zasm_test(source);
    //run_lexer_test();
    run_parser_expr_test();
    //run_parser_error_test();
    return 0;
}
