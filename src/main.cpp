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

void run_lexer_test()
{
    std::string_view source_code = R"(

    for item in items { }

    pub struct Point {
        pub x: i32,
        pub y: i32,
    }

    pub class Player {
        pub name: String,
        pub hp: i32,
    }

    pub native func move_point(pt: &mut Point, dx: i32, dy: i32) -> () {
        pt.x = pt.x + dx;
        pt.y = pt.y + dy;
    }

    pub func load_player(id: i32) -> Player ! String {
        if id <= 0 => return Err("Invalid ID\n");

        let p = new Player();
        p.name = "Hero \"Zenith\"";
        p.hp = 100;
        return Ok(p);
    }

    pub func main() -> () {
        let mut pt = Point { x: 10, y: 20 };
        let ratio = 3.14; // Decimal literal
        
        move_point(&mut pt, 5, 10);
        let x_val = pt.x;

        let struct_buf = new Point[100];
        let class_refs = new &Player[10];

        let status = if pt.x > 10 => 200 else => 400;

        let score = 'calc: {
            if status == 400 {
                break 'calc 0;
            }
            let base = 50;
            break base * 2 + 10;
        };

        let player = load_player(1)?;

        let msg = match load_player(2) {
            Ok(p) => {
                break "Success";
            },
            Err(e) => {
                break "Failed";
            }
        };
    }

    pub func demo(v: mut &Vec<i32>, flag: bool) -> (i32, bool) {
        let mut count = 0;
        let cell = mut new Cell { value: 1, next };
        let both = flag and count >= 1 or !flag;
        *slot = 2 * 3;
        return (count, both);
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

void run_parser_test() 
{
    std::string_view source_code = R"(
    'main_block: {

        let simple_int = 42;
        let decimal_val = 3.14159;
        let text_str = "Zenith Compiler";
        let typed_var: i32 = 100;
        let ref_mut_var: mut &String = &text_str;

        let neg_expr = -simple_int;
        let not_expr = !simple_int;
        let grouped = (simple_int + 10);
        let borrowed_ref = &simple_int;

        let math_res = (simple_int + 5) * 2 / 4 % 3 * 2;
        let cmp_res = math_res >= 10 == (decimal_val < 5.0);

        let age = user.profile.age;
        let first_elem = list[0];
        let calc_res = compute(math_res, borrowed_ref, "literal_arg");

        let search_res = 'search: {
            break 'search 100;
        };

        'loop_label: {
            continue;
            break 'loop_label;
        };

        process_data(search_res);

        let if_arrow = if math_res >= 10 => 1 else => 0;

        let if_block = if decimal_val < 5.0 => 100 else => 200;

        let if_chain = if simple_int < 0 => "negative" 
               else if simple_int == 0 => "zero"
               else => "positive";

        if typed_var > 50 => process_data(typed_var);

        let nested_if = if (if simple_int > 0 => true else => false) => 900 else => -1;

        struct Vec2 { x: i32, y: i32 }
        struct Marker;

        func scaled(v: &Vec2, factor: i32,) -> (i32, i32) {
            return (v.x * factor, v.y * factor);
        }

        let mut counter = 0;
        counter = counter + 1;

        let origin = Vec2 { x: 0, y: 0 };
        let handle = mut new Vec2 { x: 1, y };
        let cell = new Cell(1, "tag");

        let reader: &Vec<Vec<i32>> = &grid;
        let writer: mut &Vec2 = mut &origin;
        let pair: (i32, &Vec2) = (1, &origin);

        let unit = ();
        let single = (1,);
        let triple = (1, counter, "three");

        let ready = counter > 0 and !done or force;
        let first = *writer;
        *slot = 3;
        (*writer).x = first.x;

        let labeled_value = 'outer: {
            'inner: {
                if ready { break 'outer 1; }
            };
            break 'outer 2;
        };

        if ready { process_data(counter); }
        { process_data(first); }

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
    }
    5 + 3;
    foo() = 5;
    if x;
    if x => 1 else 42;
    'foo;
    'bar: 5;
    (;
    a.;
    a[1;
    foo(1 2);
    break x y;
    continue
    return;
    let x : = 5;
    let x: & = 5;
    let a: &mut i32 = 1;
    let b = &mut a;
    let c: mut i32 = 1;
    let d = mut a;
    break;
    if x => { y(); }
    let e = new Foo;
    let f: Vec<i32 = 1;
    foo()
    let g = 1;
    {
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
    run_lexer_test();
    //run_parser_test();
    return 0;
}