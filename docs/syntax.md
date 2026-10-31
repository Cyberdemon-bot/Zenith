# Zenith Language Syntax Specification

This document describes the concrete grammar and syntax of the Zenith programming language as implemented in the compiler parser.

---

## 1. Top Level & Declarations

A Zenith program is a sequence of statements and top-level declarations terminated by `EOF`.

```ebnf
program          := statement* EOF

statement        := ";"* 
                  ( func_decl
                  | struct_decl
                  | let_stmt
                  | return_stmt
                  | break_stmt
                  | continue_stmt
                  | assign_stmt
                  | expr_stmt
                  )
```

### Function Declarations
Functions can optionally be exported with `pub` and declare parameters and return types.

```ebnf
func_decl        := "pub"? "func" IDENT "(" param_list? ")" ( "->" type_annotation )? block_expr
param_list       := param ( "," param )* ","?
param            := IDENT ":" type_annotation
```

*Example:*
```zenith
pub func scaled(v: &Vec2, factor: i32) -> (i32, i32) {
    return (v.x * factor, v.y * factor);
}

func noop() {}
```

### Struct Declarations
Structs support either unit form (ending in `;`) or field definitions inside `{}`.

```ebnf
struct_decl      := "pub"? "struct" IDENT ( ";" | "{" field_list? "}" )
field_list       := field ( "," field )* ","?
field            := IDENT ":" type_annotation
```

*Example:*
```zenith
struct Marker;
struct Empty {}
pub struct Vec2 {
    x: i32,
    y: i32,
}
```

---

## 2. Statements

### Variable Declarations (`let`)
Variables are immutable by default unless declared with `mut`. Type annotations are optional.

```ebnf
let_stmt         := "let" "mut"? IDENT ( ":" type_annotation )? "=" expr ";"
```

*Example:*
```zenith
let a = 42;
let typed: i32 = 1;
let mut counter = 0;
```

### Control Statements
*Note:* Unlabeled `break` is not allowed; a target label is mandatory.

```ebnf
return_stmt      := "return" expr? ";"
break_stmt       := "break" LABEL expr? ";"
continue_stmt    := "continue" ";"
```

*Example:*
```zenith
return (v.x, v.y);
break 'outer 42;
break 'loop;
continue;
```

### Assignments
Assignment targets must be identifiers, field accesses, index expressions, or dereferences.

```ebnf
assign_stmt      := assign_target "=" expr ";"
assign_target    := IDENT | field_access | index_expr | deref_expr
```

*Example:*
```zenith
counter = counter + 1;
point.x = 10;
items[0] = 42;
*ptr = 100;
```

### Expression Statements
Only calls or block-like expressions (`{...}`, `if`, `for`, labeled expressions) are valid statements. Block-like expressions closed by `}` do not require a trailing semicolon.

```ebnf
expr_stmt        := block_like_expr ";"?
                  | call_expr ";"
```

---

## 3. Type Annotations

Zenith supports named types (with optional generic type arguments), references, and tuple types.

```ebnf
type_annotation  := named_type
                  | ref_type
                  | tuple_type

named_type       := IDENT ( "<" type_arg_list ">" )?
type_arg_list    := type_annotation ( "," type_annotation )* ","?

ref_type         := "mut"? "&" type_annotation
tuple_type       := "(" ( type_annotation ( "," type_annotation )* ","? )? ")"
```

*Important:* Mutable references use prefix `mut &T` (writing `&mut T` produces a syntax error).

*Example:*
```zenith
i32
Vec<i32>
&Vec2
mut &Vec2
(i32, i32)
()
```

---

## 4. Expressions

Zenith uses Pratt precedence climbing for binary and postfix operations.

```ebnf
expr             := prefix_expr ( infix_op expr )*
```

### Literals & Atoms
```ebnf
literal          := INTEGER
                  | DECIMAL
                  | STRING
                  | "true" | "false" | "null"
                  | tuple_literal
```

*Tuples:*
* `()` is the unit tuple.
* `(expr)` is parenthesized grouping.
* `(expr,)` or `(expr, expr, ...)` is a tuple expression.

### Prefix Expressions
```ebnf
prefix_expr      := literal
                  | IDENT
                  | "!" expr
                  | "-" expr
                  | "*" expr                          (* deref_expr *)
                  | "(" expr ")"                      (* paren_expr or tuple *)
                  | "&" expr                          (* immutable borrow *)
                  | "mut" "&" expr                    (* mutable borrow *)
                  | "new" named_type constructor_args (* heap allocation *)
                  | "mut" "new" named_type constructor_args
                  | struct_init
                  | block_like_expr

constructor_args := "(" arg_list? ")"
                  | "{" field_init_list? "}"
```

*Notes:*
* Mutable borrows must be written as `mut &x` (writing `&mut x` produces a syntax error).
* Heap allocations with `new` can take either constructor argument lists `( ... )` or field initializer lists `{ ... }`.

### Struct Initializers
Struct literals support explicit field initialization and shorthand field punning (`x` expands to `x: x`).

```ebnf
struct_init      := IDENT "{" field_init_list? "}"
field_init_list  := field_init ( "," field_init )* ","?
field_init       := IDENT ( ":" expr )?
```

*Example:*
```zenith
let origin = Vec2 { x: 0, y: 0 };
let shorthand = Vec2 { x, y };
let empty = Empty {};
```

### Block-Like Expressions
Block-like expressions can appear anywhere an expression is expected.

```ebnf
block_like_expr  := block_expr
                  | labeled_expr
                  | if_expr
                  | for_expr

block_expr       := "{" statement* "}"

labeled_expr     := LABEL ":" ( block_expr | if_expr | for_expr )

if_expr          := "if" expr ( "=>" expr | block_expr ) 
                    ( "else" ( if_expr | "=>" expr | block_expr ) )?

for_expr         := "for" block_expr                             (* infinite loop *)
                  | "for" expr block_expr                        (* while loop *)
                  | "for" IDENT "in" expr block_expr             (* for-in loop *)
```

*Example:*
```zenith
// Block & Label
let v = 'outer: {
    if ready { break 'outer 1; }
    break 'outer 2;
};

// If expressions (arrow or block bodies)
let a = if x > 0 => 1 else => 0;
if ready { process(x); } else if other { process(y); } else { process(z); }

// For loops
for { process(1); }
for ready { process(x); }
for item in items { process(item); }
for item in &list { process(item); }
for item in mut &list { process(item); }
'scan: for item in items { if item == target { break 'scan; } }
```

### Infix Operations
```ebnf
infix_op         := binop
                  | "." IDENT                         (* field access *)
                  | "(" arg_list? ")"                 (* call *)
                  | "[" expr "]"                      (* index *)

arg_list         := expr ( "," expr )* ","?

binop            := "or"
                  | "and"
                  | "==" | "!="
                  | "<"  | "<=" | ">" | ">="
                  | "+"  | "-"
                  | "*"  | "/"  | "%"
```

---

## 5. Operator Precedence (Low to High)

Precedence is strictly enforced during expression climbing:

| Level | Precedence Name | Operators | Associativity | Description |
| :---: | :--- | :--- | :---: | :--- |
| **0** | `P_LOWEST` | | | Lowest base precedence |
| **1** | `P_OR` | `or` | Left | Logical OR |
| **2** | `P_AND` | `and` | Left | Logical AND |
| **3** | `P_EQUALS` | `==`, `!=` | Left | Equality comparisons |
| **4** | `P_LESSGREATER` | `<`, `<=`, `>`, `>=` | Left | Relational comparisons |
| **5** | `P_SUM` | `+`, `-` | Left | Addition, subtraction |
| **6** | `P_PRODUCT` | `*`, `/`, `%` | Left | Multiplication, division, remainder |
| **7** | `P_UNARY` | `!`, `-`, `*` (deref), `&`, `mut &`, `new`, `mut new` | Right | Unary prefix operators |
| **8** | `P_CALL` | `.`, `(args)` | Left | Member access, function call |
| **9** | `P_INDEX` | `[index]` | Left | Array / collection indexing |
