// src/main.rs
pub mod token;
pub mod lexer;
pub mod ast;
mod parser;

use std::env;     
use std::fs;     
use std::process; 

use lexer::Lexer;
use parser::Parser;
use token::TokenType; 
fn main() 
{
    let debug_lexer_only: bool = false; 

    let args: Vec<String> = env::args().collect();

    if args.len() < 2 
    {
        println!("❌ Error: missing file path!");
        process::exit(1);
    }

    let file_path = &args[1];
    println!("--- READING SOURCE CODE: {} ---", file_path);

    let file_content = match fs::read_to_string(file_path) {
        Ok(content) => content,
        Err(err) => {
            println!("Cannot read file '{}': {}", file_path, err);
            process::exit(1);
        }
    };

    if debug_lexer_only {
        println!("--- (LEXER ONLY DEBUG) ---");
        let mut debug_lexer = Lexer::new(&file_content);
        let mut token_count = 0;

        loop 
        {
            let tok = debug_lexer.next_token();
            println!("Token [{}]: {:?}", token_count, tok);
            token_count += 1;

            if tok.token_type == TokenType::EOF {
                break;
            }
        }
        println!("--- COMPLETED ---");
        return; 
    }

    println!("--- (PARSING) ---");

    let lexer = Lexer::new(&file_content);
    let mut parser = Parser::new(lexer);
    let program = parser.parse_program();

    if !parser.errors.is_empty() 
    {
        println!("DETECTED INVALID SYNTAX");
        for err in parser.errors 
        {
            println!("  {}", err);
        }
        process::exit(1);
    }

    let json_str = match serde_json::to_string_pretty(&program) {
        Ok(str_data) => str_data,
        Err(e) => {
            println!("CANNOT CREATE JSON: {}", e);
            process::exit(1);
        }
    };

    let output_file = "ast.json";
    match fs::write(output_file, &json_str) {
        Ok(_) => {
            println!("--- CREATED JSON FILE SUCCESSFULLY ---");
        }
        Err(err) => {
            println!("❌ JSON FILE ERROR '{}': {}", output_file, err);
            process::exit(1);
        }
    }
}