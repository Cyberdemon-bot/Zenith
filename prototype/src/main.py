#!/usr/bin/env python3.12
"""CLI entry point for the Zenith prototype tree-walking interpreter."""

import argparse
import os
import subprocess
import sys
from pathlib import Path

# Ensure sibling modules in `prototype/src` are importable regardless of current working directory
SRC_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SRC_DIR.parents[1]
SCRIPT_DIR = PROJECT_ROOT / "prototype" / "script"

if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from ast_nodes import Program, build_ast  # noqa: E402
from evaluator import Evaluator  # noqa: E402
from sexpr import parse_sexpr  # noqa: E402
from signals import ZenithRuntimeError  # noqa: E402


def resolve_script_path(raw_path: str) -> Path:
    """Resolves a `.zen` script path relative to CWD, PROJECT_ROOT, or `prototype/script`."""
    candidate = Path(raw_path)
    if candidate.is_file():
        return candidate.resolve()

    from_root = PROJECT_ROOT / raw_path
    if from_root.is_file():
        return from_root.resolve()

    from_script_dir = SCRIPT_DIR / raw_path
    if from_script_dir.is_file():
        return from_script_dir.resolve()

    raise FileNotFoundError(f"Zenith script not found: '{raw_path}'")


def fetch_ast_from_parser(zen_file: Path) -> str:
    """Runs `xmake run main <zen_file>` from PROJECT_ROOT and captures the S-expression AST."""
    env = os.environ.copy()
    extra_paths = ["/opt/homebrew/bin", "/usr/local/bin"]
    current_path = env.get("PATH", "")
    env["PATH"] = ":".join(extra_paths + ([current_path] if current_path else []))

    result = subprocess.run(
        ["xmake", "run", "main", str(zen_file)],
        cwd=str(PROJECT_ROOT),
        env=env,
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        err_msg = result.stderr.strip() or result.stdout.strip()
        raise ZenithRuntimeError(f"Parser failed on '{zen_file.name}':\n{err_msg}")

    # Filter out any xmake build status lines if xmake recompiled target before running
    lines = result.stdout.splitlines()
    ast_start_idx = 0
    for i, line in enumerate(lines):
        if line.lstrip().startswith("(Program"):
            ast_start_idx = i
            break
    return "\n".join(lines[ast_start_idx:])


def run_sexpr_source(sexpr_text: str, dump_ast: bool = False) -> None:
    """Parses S-expression text and evaluates the program."""
    if dump_ast:
        print("=== S-Expression AST ===")
        print(sexpr_text.strip())
        print("========================")

    snode = parse_sexpr(sexpr_text)
    ast_root = build_ast(snode)
    if not isinstance(ast_root, Program):
        raise ZenithRuntimeError(f"Expected root Program node, got {type(ast_root).__name__}")

    evaluator = Evaluator()
    evaluator.run_program(ast_root)


def run_file(raw_path: str, dump_ast: bool = False) -> bool:
    """Runs a single `.zen` file and returns True on success."""
    try:
        zen_path = resolve_script_path(raw_path)
        sexpr_text = fetch_ast_from_parser(zen_path)
        run_sexpr_source(sexpr_text, dump_ast=dump_ast)
        return True
    except (FileNotFoundError, ZenithRuntimeError) as err:
        print(f"[Zenith Error] {err}", file=sys.stderr)
        return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Zenith Prototype Tree-Walking Interpreter (Python 3.12)"
    )
    parser.add_argument(
        "script",
        nargs="?",
        help="Path to a .zen script file (resolved from CWD, project root, or prototype/script)",
    )
    parser.add_argument(
        "--stdin",
        action="store_true",
        help="Read S-expression AST directly from standard input",
    )
    parser.add_argument(
        "--dump-ast",
        action="store_true",
        help="Print the S-expression AST before executing",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Run all .zen test scripts inside prototype/script/",
    )

    args = parser.parse_args()

    if args.stdin:
        sexpr_input = sys.stdin.read()
        try:
            run_sexpr_source(sexpr_input, dump_ast=args.dump_ast)
            return 0
        except ZenithRuntimeError as err:
            print(f"[Zenith Error] {err}", file=sys.stderr)
            return 1

    if args.all:
        scripts = sorted(SCRIPT_DIR.glob("*.zen"))
        if not scripts:
            print(f"No .zen scripts found in {SCRIPT_DIR}", file=sys.stderr)
            return 1
        passed = 0
        for s in scripts:
            print(f"--- Running {s.name} ---")
            if run_file(str(s), dump_ast=args.dump_ast):
                passed += 1
            print()
        print(f"Summary: {passed}/{len(scripts)} scripts succeeded.")
        return 0 if passed == len(scripts) else 1

    if not args.script:
        parser.print_help()
        return 1

    return 0 if run_file(args.script, dump_ast=args.dump_ast) else 1


if __name__ == "__main__":
    sys.exit(main())
