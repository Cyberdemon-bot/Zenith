# Zenith Language Semantics

**Last updated:** 2026-11-22

---

## 1. Philosophy

### 1.1 Core principles

- **"No magic"** — every abstraction has visible cost; every effect is traceable.
- **Go-like ergonomics** — simple surface, transparent depth.
- **Byte-identical** — struct layout matches native ABI (FFI-first).
- **Hybrid VM + native** — bytecode is default; native is acceleration.

### 1.2 Explicit cost

- No hidden allocation, hidden dispatch, or hidden conversion.
- Implicit behaviors are forbidden unless documented and bounded.
- `#[native]` is explicit — never compiler auto-detected.

### 1.3 Hybrid execution model

| Layer | Description |
|---|---|
| VM | Register-based bytecode interpreter (default) |
| Native | Startup-JIT/AOT for `#[native]`-marked functions |
| Boundary | FFI bridge with explicit marshaling |

Every feature must work in **3 states**: bytecode-only, native-only, bridged.

---

## 2. Type System

### 2.1 Static typing & local constraint-based inference

- Types are resolved at compile time.
- Inference is **local** (per-function bidirectional typing + local `ConstraintSolver`); no global Hindley-Milner inference.
- Supported Phase 3 scalar types: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `bool`, and unit `()`.
  - The unit type and unit value are represented by the empty tuple **`()`**.
- **Constraint resolution pipeline (per function):**
  1. **Pass 1 (`Eq`):** Unify all equality constraints.
  2. **Pass 2 (Fixed-point propagation):** Iteratively solve `UnaryOp`, `BinaryOp`, `Cast`, `IntLit`, and `FloatLit` constraints until no further progress.
  3. **Numeric literal fallback:** Default one unresolved numeric literal at a time (**`i64`** for integers, **`f64`** for floats) and re-run Pass 2 so linked constraints converge before defaulting remaining literals.
  4. Any type variable still unresolved after solving reports `CannotInferType`.
- **Literal range checking & negative folding:**
  - Numeric literals are range-checked against their resolved target type (`LiteralOutOfRange`).
  - Unary `-` applied directly to an integer literal is folded prior to range checking so signed minimum bounds (`-128` for `i8`, `-32768` for `i16`, `-2147483648` for `i32`) are accepted without overflow.
- **Explicit casts only:** `expr as T` — no implicit widening or narrowing.

```zenith
let x = 5;           // x: i64 (default integer literal)
let y = 3.14;        // y: f64 (default float literal)
let z: i8 = -128;    // folded negative literal, valid i8 min bound
let w = z as i64;    // explicit cast
```

### 2.2 Operators & primitive casts

All binary operators require both operands to have the **exact same type** (no implicit promotion).

| Group | Operators | Allowed operand types | Result type |
|---|---|---|---|
| Arithmetic | `+`, `-`, `*`, `/` | Any numeric (`i8`–`i64`, `u8`–`u64`, `f32`, `f64`) | Same as operand |
| Modulo | `%` | Integer only (`i8`–`i64`, `u8`–`u64`) | Same as operand |
| Equality | `==`, `!=` | Numeric or `bool` (`unit` disallowed) | `bool` |
| Ordering | `<`, `<=`, `>`, `>=` | Numeric only | `bool` |
| Logical | `and`, `or`, `!` | `bool` only (`and`/`or` short-circuit) | `bool` |
| Unary negation | `-` | Signed numeric (`i8`–`i64`, `f32`, `f64`; unsigned disallowed) | Same as operand |

**Primitive cast (`expr as T`) rules:**
- **Allowed:** `T -> T`, `numeric <-> numeric`, `bool <-> integer`.
- **Disallowed:** `float <-> bool`, and any cast involving `unit` (`CoerceFailure`).

**Phase 3 runtime arithmetic & conversion semantics:**
- **Integer overflow:** Two's complement **wrapping** for `+`, `-`, `*`, and unary `-`.
- **Small integers (`i8`, `i16`, `u8`, `u16`):** Stored in 32-bit register slots (zero-extended to 64-bit `Value`) and truncated via wrapping `CastKind::TruncI8/I16/U8/U16`.
- **Division & remainder traps:** Integer division/remainder by `0` traps with `DivisionByZero`; signed `INT_MIN / -1` and `INT_MIN % -1` trap with `IntegerOverflow`.
- **Floating-point comparisons:** Full IEEE-754 `NaN` semantics (backed directly by VM `Eq`, `Ne`, `Lt`, `Le`).
- **`float as int` edge cases:** Converting `NaN`, `Inf`, or out-of-range floats to integers is **unspecified in Phase 3** (saturating casts planned for a later phase).

### 2.3 Variables, mutability & scoping

- `let` bindings are **immutable by default**; reassignment requires `mut` (`ImmutableAssignment` otherwise).
- **Shadowing** is allowed in nested block scopes; redeclaring an identifier in the same scope is a compile error (`RedeclaredIdentifier`).
- **Functions** are top-level only in Phase 3: duplicate function names are rejected (`DuplicateFunction`), and nested functions are rejected (`UnsupportedFeature`).

### 2.4 Type categories

| Category | Storage | Copy semantics | Identity |
|---|---|---|---|
| **Value (primitive)** | Register | Deep | No |
| **Value (struct)** | Register block / arena | Deep | No |
| **Value (array wrapper)** | Register (ptr+len) | Shallow (share data) | No |
| **Reference (class)** | Register (ptr) | Shallow (same object) | Yes |
| **Borrowed (`&T`)** | Register (ptr) | Shallow | No |

### 2.5 Null

- **Not present in Phase 3.**
- Parser may accept `null` literal; compiler rejects with error.
- Future semantics TBD.

### 2.6 Generics & traits

- **Deferred to Phase 4+.**
- `dyn Shape` — trait object, interface-like.
- Generic functions — syntax TBD.

### 2.7 Type consistency at merge points

**Principle:** every control flow path reaching a program point must carry the same type.

- Break values in a breakable construct: all-or-none and same type (mixed = compile error).
- If-else branches: must unify to the same type (or `unit` when `else` is omitted).
- Return statements: must match the function's return type.

---

## 3. Memory Model

### 3.1 Storage tiers

| Tier | Storage | Used for | GC scan |
|---|---|---|---|
| Register | `Value[256]` per frame | Scalars, small structs | No |
| Arena | Frame-local byte buffer | Structs > 32B, escape | No |
| GC heap | VM-managed | Arrays, strings, objects | Yes |

### 3.2 Struct representation

**Semantic guarantee:** struct is **byte-identical** to native ABI.

- `sizeof`, `offsetof`, `alignof` follow C rules.
- Padding between fields per natural alignment.
- `&struct` yields a valid pointer for FFI, memcpy, serialization.

**Implementation tiers:**

- Struct ≤ 32 bytes, non-escaping → **register block** (N consecutive slots).
- Struct > 32 bytes, or escaping → **arena** (frame-local byte offset).
- Scalarization (SROA) — deferred optimization.

### 3.3 Array

```zenith
let a = i32[100];      // array<i32>, value wrapper {ptr, len}, data on heap
let b = i32[n];        // runtime size n
```

- **Array is a value type** — wrapper `{ptr, len}` is a 2-word value.
- **Data is GC-managed** on heap.
- **Copy is shallow** — copies wrapper, shares data.
- **No `new T[n]`** — `T[n]` is the only allocation syntax for arrays.
- **Fixed size** — no grow after allocation.
- **Bounds check** always runtime; panic on OOB.
- **Sub-slicing** deferred to Phase 4.
- **OOM handling** — panic with message.

### 3.4 Vec

- **`Vec<T>` is a library type**, not primitive.
- Layout: `{ buf: array<T>, len: usize }` — 3-word value.
- `push` grows by reallocating an array of 2× capacity.
- **Copy shares buffer**; `len` is independent per copy.

### 3.5 String

- **`String` is a library type** (T3 stdlib).
- **Literal is borrowed slice** `&[u8]` — zero allocation.
- Owned string explicit: `String::from(...)` or `.to_string()`.

```zenith
let s = "hello";              // &[u8], borrowed, zero alloc
let owned = s.to_string();    // String, owned, 1 alloc
```

- **No SSO, no interning** in Phase 3.
- **No native support** — String is pure bytecode implementation.

### 3.6 Dict

- **`Dict<K, V>` is a library type** (T3 stdlib).
- Syntax: `{ "key": value, ... }` — Python-style.
- Empty dict: `{}` in expression context.
- Disambiguation: after `=`, `{` starts dict; in statement position, `{` starts block.
- Implementation: open addressing (VM-aware, not chaining).

### 3.7 Class

- **Reference type, heap-allocated, GC-managed.**
- **Has identity** — reference comparison, not structural.
- Instantiated with `new Foo(args)`.
- Syntax details TBD (deferred to Phase 4).

### 3.8 Allocation syntax

| Syntax | Meaning | Return |
|---|---|---|
| `T[n]` | Array allocation | `array<T>` (value) |
| `new Foo(args)` | Class instantiation | `&Foo` (reference) |
| `new Foo at ptr` | Placement construction | `&Foo` (Phase 4+) |

**`new` is only for object construction**, never for memory allocation.

---

## 4. Control Flow

### 4.1 Blocks

- **Value defaults to `unit`**; there is no implicit last-expression value.
- **Value via labeled break:** `break 'label value;`
- A block without any `break` in a context expecting a non-`unit` type is a compile error (`BlockMissingBreak`).

```zenith
let v = 'outer: {
    if ready { break 'outer 1; }
    break 'outer 2;
};
```

### 4.2 If

**Two syntaxes, one `IfExpr` AST node** (handled directly by `TypeChecker` and `CodeGen`, with no separate AST desugaring pass):

```zenith
// Arrow form (single-expression body)
let x = if c => 1 else => 2;
if ready => process(x); // valid because process(x) has type unit

// Block form
if c { do_something(); }
```

**Rules:**
- Condition must have type `bool`.
- Arrow form's body must be a **single expression** (not a `{ }` block).
- When `else` is **present**, both branches must unify to the same type.
- When `else` is **omitted** (in either block or arrow form), the `IfExpr` has type **`unit`**; if the `then` branch produces a non-`unit` type (e.g. `let a = if c => 5;`), the compiler rejects it with `TypeMismatch`.
- `else if` nests `IfExpr` directly.
- `else` without `if` or `{ }` → error.

### 4.3 For loops

**No `while` keyword. No C-style for.**

| Syntax | Equivalent | Value type |
|---|---|---|
| `for cond { }` | while loop | Always **`unit`** (`break value;` is a compile error, as the loop may execute 0 times) |
| `for { }` | infinite loop | Type of its `break value;` (or `unit` if no value break; missing `break` in a non-`unit` context → `BlockMissingBreak`) |
| `for x in iter { }` | iterator protocol | Deferred to Phase 4 |

### 4.4 Break & continue

| Form | Behavior |
|---|---|
| `break;` | Nearest enclosing `for` loop (unlabeled `break` does not target a labeled block) |
| `break value;` | Exit nearest enclosing `for {}` loop with value |
| `break 'label;` | Exit labeled construct (`for`, `if`, or block) |
| `break 'label value;` | Exit labeled construct with value |
| `continue;` | Nearest `for` loop |
| `continue 'label;` | Labeled `for` loop |

**Consistency rule:** within one breakable construct, all breaks must carry the same type (or all carry no value / `unit`). Mixed = compile error (`BreakTypeMismatch`).

### 4.5 Labeled constructs

Any of `BlockExpr`, `IfExpr`, `ForExpr` can be labeled:

```zenith
'outer: { break 'outer 1; }
'maybe: if ready { break 'maybe; }
'forever: for { break 'forever; }
```

**Labeled construct = breakable scope.** `continue` is only valid when targeting an enclosing `for` loop (`InvalidContinue`).

---

## 5. Functions & Calling Convention

### 5.1 Register window convention

- **Arguments:** caller places at `regs[a..a+N-1]`, where `N = callee.NumParams`.
- **Return values:** callee places at `regs[0..M-1]`, which aliases `caller.regs[a..a+M-1]` via register window overlap (`Return a, count, 0` copies `regs[a..a+count-1]` to `regs[0..count-1]` before popping the frame).
- **Caller reserves** `max(N, M)` consecutive registers starting at `a`.

### 5.2 Multi-return (Phase 3.5)

```zenith
func divmod(a: i64, b: i64) -> (i64, i64) {
    return (a / b, a % b);
}
```

- Encoded as `Return a, count, 0` where `count` = number of return values (`0` for `unit`, `1` for Phase 3 scalar returns).
- Forward-copy safety: `a > 0` implies safe forward iteration.

### 5.3 Main function

- Every program **must define `func main`** (`MissingMainFunction`).
- **Valid signature:** 0 parameters, return type must be **`unit`** or **`i64`** (`InvalidMainSignature`).
- `Return` executed in the outermost frame (`frame_depth == 1`) halts execution cleanly while keeping the main frame alive for return-value inspection at `R[0]` (`Halt` is also supported).

---

## 6. Borrowing & References

### 6.1 Reference syntax

- `&T` — immutable reference
- `&mut T` — mutable reference (`&mut x` in expressions)
- `*T` — raw pointer (FFI, unsafe)

### 6.2 Dereferencing

- **No auto-deref.** Explicit `*p` required.
- `(*p).field` — must write explicit deref before field access.

### 6.3 Borrow checker

- **Compile-time** checking (deferred to Phase 4).
- **Simplified** for Phase 3 — no borrow checking at all.
- **Gradual lifetime** (Phase 5+): plain `&T` for internal, `Maybe<&T>` for boundary.

---

## 7. Unsafe Code

### 7.1 Unsafe block semantics

```zenith
unsafe {
    // GC does not collect any allocation in this block
    // Escape = Undefined Behavior
    // Compiler warns if escape detected, but does not reject
}
```

- **Allocation protection:** every allocation inside unsafe block is pinned (GC skips).
- **Escape = UB:** if object escapes block, dangling after block ends. VM may crash, corrupt, or work by chance.
- **Warning only:** compiler emits warning for detectable escape, does not fail compilation.
- **No runtime check:** no overhead; user bears responsibility.

### 7.2 Placement new

```zenith
unsafe {
    let buf = u8[sizeof(Foo)];
    let f = new Foo { ... } at buf;
    // f valid only within unsafe block
}
```

- **POD types only** for safe placement (compiler enforces in Phase 4+).
- **Non-POD requires `unsafe`** — GC may not scan raw memory.
- **Pinning** scoped to block; escape = UB.

### 7.3 When to use unsafe

- FFI boundary (wrap C pointers)
- Custom allocators
- Manual memory layout
- **Never** for "avoid borrow checker" — use structured code instead.

---

## 8. Native Code

### 8.1 Marking

```zenith
#[native]
func fast_hash(data: &[u8]) -> u64 { ... }
```

- **Explicit marking only** — no auto-detection.
- **Justification required** (comment convention): hardware access, hot path (profile-driven), or FFI.
- **Bytecode fallback required** — same function must have bytecode implementation for portability/debug.

### 8.2 Stdlib rule

- **Stdlib is 100% bytecode.** No `#[native]` in stdlib (CI enforced).
- Exception whitelist: syscall, atomic, SIMD builtin (~50 functions).
- If stdlib is not fast enough → fix algorithm, not add native.

### 8.3 Compilation pipeline

- Startup JIT/AOT for `#[native]` functions.
- Compile in background thread; block first call only if needed.
- **Cache compiled code to disk** (`.znc-cache`), reload on next startup.
- **Fallback to bytecode** on compile failure.

### 8.4 Target ratios

- Bytecode: **85-95%** of user code.
- Native: **5-15%** — hardware access, hot paths, FFI.
- Native must never be the **only** implementation of a feature.

---

## 9. Compilation Strategy

### 9.1 Phase 3 pipeline

```
Source
  ↓ [Lexer + Parser]    immutable flat DOD AST (AstView)
AST
  ↓ [TypeChecker]       signature collection, bidirectional check + ConstraintSolver, break/scope validation
TypedAstView
  ↓ [CodeGen]           single-pass tree-walker (handles arrow if / else-if directly)
Frozen Module (Bytecode)
```

- **No intermediate representation (IR), no separate AST desugar pass, no CFG.**
- **No optimization** in Phase 3 (constant folding, peephole deferred to Phase 3.5+).
- **Unified Diagnostics (`Diagnostics.def`):** Parser (`100–105`), TypeChecker (`200–291`), CodeGen (`300–302`: `RegisterOverflow`, `ConstantPoolOverflow`, `JumpOffsetOverflow`).

### 9.2 Register allocator

- **2-tier LIFO stack** — `m_LocalTop` (parameters and local variables scoped by block depth) + `m_RegTop` (short-lived expression temporaries freed immediately after use), tracking `m_MaxRegs`.
- Destination-driven `CompileExpr(node, dst)` avoids redundant `Mov` instructions.
- Max 255 registers per function (`RegisterOverflow` if exceeded).

### 9.3 Metadata preservation (Deferred to Phase 3.5 / 4+)

Not emitted in Phase 3's 28-byte `Proto`. Planned for Phase 3.5 / 4+ debugging and optimization:
- **Line map** (pc → source line)
- **Loop metadata** (range, header, exit)
- **Variable bindings** (name → register per pc range)

### 9.4 Bytecode format

- Register-based Three-Address Code (TAC), **not SSA** (registers can be reassigned).
- 32-bit instruction: 8-bit dispatch slot (`0..255`), 24-bit operands (`iNone`, `iABC`, `iABx`, `iAsBx`, `iAx`).
- **57 / 256 dispatch slots** currently used across 24 opcodes:
  - **Slot Aliasing:** `I32→U32`, `I64→U64`, `REF→U64` on `Add`, `Sub`, `Mul`, `Eq`, `Ne`.
  - **Signed/Unsigned split:** `DivS`/`DivU`, `RemS`/`RemU`, `Lt`, `Le`, `Neg`.
  - **Floating-point:** `Div` (`F32`, `F64`), plus `F32`/`F64` variants on `Add`/`Sub`/`Mul`/`Neg`/`Eq`/`Ne`/`Lt`/`Le`.
  - **Type conversions:** single-slot `Opcode::Cast` (`iABC`) with 25 `CastKind` sub-opcodes.
- **32-bit zero-extension invariant:** `Value::SetAs<Type::I32/U32/F32>` zero-extends into `u64`. Because `LoadImm` sign-extends `int16_t` into `i64`, `CodeGen` only uses `LoadImm` for `i64` or non-negative `0..32767` values of `<= 32-bit` types (negative `i8`/`i16`/`i32` constants go through `LoadConst`).

### 9.5 Module & Proto

- **Module** owns frozen bytecode, constant pool buffers, and single-assignment `EntryProto` (`SetEntry`/`GetEntry`/`HasEntry`).
- **Proto** is pure metadata (28 bytes): code/const offsets & counts, `FirstChild`/`ChildCount`, `NumParams`, `NumReturn`, `MaxRegs`.
- **Module is immutable after `Freeze()`** — safe to share across `State` instances and threads.

---

## 10. Error Handling & Runtime Traps

### 10.1 User-level error handling

- **Not present in Phase 3.**
- Phase 3.5: multi-return (Go-style `(T, Error)`).
- No exceptions, no panic-to-catch in Phase 3.
- `null` = unimplemented (deferred).

### 10.2 VM runtime traps (Phase 3)

The VM enforces runtime safety traps without crashing the host process:
- `DivisionByZero` — integer `/` or `%` by `0`.
- `IntegerOverflow` — signed `INT_MIN / -1` or `INT_MIN % -1` (`DivS`/`RemS`).
- `CallStackOverflow` — call frame depth limit exceeded.
- `RegisterStackOverflow` — register stack capacity exceeded.

On a trap, `Executor::Execute()` halts immediately, returns `false`, and stores an 8-byte `RuntimeError` (`code`, `ip`) in `State`.

---

## 11. Phase Plan

| Phase | Scope | Est. time |
|---|---|---|
| **3** | Scalar compiler: `i8`–`u64`/`f32`/`f64`/`bool`/`unit`, `as` cast, basic control flow, single-return function call | 5 weeks |
| **3.5** | Struct (register block + arena), tuple/multi-return, array, string, dict, error handling, debug metadata | 3-4 weeks |
| **4** | Borrow checker, for-in, iterator protocol, methods, match, class | 2 months |
| **5** | GC, unsafe block, FFI bridge, generic, trait/dyn | 3 months |
| **6** | PGO (`--profile=trace.json`), speculation, `#[native]` JIT | 2 months |
| **7** | Full JIT (copy-and-patch or template) | 3-6 months |

### Phase 3 scope (strict)

**IN:**
- Types: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `bool`, unit `()`
- Expressions: literal, identifier, paren, unary, binary, `as` cast, call, block, if (block & arrow form), for, labeled block/if/for
- Statements: `let` (typed & untyped, `mut`), `assign` (identifier), `return`, `break`, `continue`, expr-stmt
- Control: `if` (block & arrow form), `for` (`for cond` / `for {}`), labeled block/`if`/`for`
- Functions: top-level declaration + single-return call (tuple/multi-return deferred to Phase 3.5)

**OUT (defer):**
- Struct instantiation, field access
- Tuple / multi-return
- Array, `T[n]`
- String, string literal
- Dict
- Borrow, deref
- For-in, iterator
- Match, pattern
- Class, `new`
- Generic, trait, dyn
- Unsafe block, placement new
- GC
- Native marking & JIT

**Success criteria:**
- 4 benchmark programs compile and run correctly: `fib(n)`, `gcd(a, b)`, `prime_counting(n)`, `factorial(n)`
- Disassemble round-trip: compile → disassemble → assemble → run produces identical results
- Type/control errors detected at compile time (mixed `break`, non-`unit` `if` without `else`, `break value` in `for cond`, literal out-of-range, invalid casts)
- Runtime traps caught cleanly (`DivisionByZero`, `IntegerOverflow`, stack overflow)

---

## 12. Open Questions (non-blocking for Phase 3)

- **Class semantics:** inheritance, method dispatch, nullability — Phase 4 decision.
- **Match patterns:** need `Pattern` node — Phase 4 decision.
- **Error handling:** Go-style multi-return vs `Result<T, E>` — Phase 3.5 decision.
- **Trait syntax:** `trait Shape { ... }` — Phase 5 decision.
- **Generic function:** `func id<T>(x: T) -> T` — Phase 5 decision.
- **Static memory:** `static BUF: u8[N]` — Phase 5 decision.
- **Array sub-slicing:** `a[i..j]` — Phase 4 decision.

---

## 13. Design Invariants

These must never be violated:

1. **Byte-identical structs** — layout matches native ABI always.
2. **`new` only for object construction** — never for memory allocation.
3. **Stdlib is pure bytecode** — no `#[native]` in shipped library.
4. **Unsafe escape is UB** — no runtime check, user responsibility.
5. **Break value consistency** — no mixed value/no-value in the same breakable construct.
6. **Missing `else` yields `unit`** — a non-`unit` `then` branch without `else` (block or arrow form) is a compile-time type error.
7. **No auto-deref** — explicit `*p` always required.
8. **Value types have no identity** — comparison by content, not reference.
9. **Module is immutable after `Freeze()`** — safe to share.
10. **Type consistency at merge points** — every path carries the same type.

---

## 14. Changelog

### 2026-11-22 (Supersedes `sematic_11022026.md`)
- Consolidated into a single self-contained `docs/semantics.md` matching the implemented Lexer, Parser, TypeChecker, and VM.
- Documented the 2-pass `ConstraintSolver` pipeline, negative literal folding, operator/cast compatibility matrix, wrapping integer arithmetic, small-integer truncation, and Phase 3 `float as int` edge-case policy.
- Updated `IfExpr` rules (arrow form allows omitted `else` when the `then` branch is `unit`; handled directly without an AST desugar pass) and `for cond` rules (always `unit`, disallowing `break value;`).
- Clarified that empty tuple `()` represents the unit type and value (no `unit` named type; non-empty tuples deferred to Phase 3.5), and documented `mut`/shadowing/top-level function rules and `func main` signature requirements.
- Synced VM & bytecode specs: 57/256 dispatch slots, Slot Aliasing, `Opcode::Ne`/`Opcode::Le` (full IEEE-754 float comparisons), `Opcode::Cast` (25 `CastKind`), `Module::EntryProto`, `Proto::NumReturn`, `LoadImm` 32-bit zero-extension invariant, and VM runtime traps (`DivisionByZero`, `IntegerOverflow`, stack overflows).
- Marked CodeGen debug metadata (line map, loop metadata, variable bindings) as deferred to Phase 3.5 / 4+.
