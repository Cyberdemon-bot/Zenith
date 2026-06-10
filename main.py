import json
import sys
from evaluator import Evaluator

def main():
    sys.setrecursionlimit(50000)
    with open("ast.json", "r", encoding="utf-8") as f:
        ast_data = json.load(f)

    evaluator = Evaluator()
    print("--- STARTING CORE GLOBAL EVALUATION ---")
    main_return, global_memory = evaluator.execute_program(ast_data)

    print("\n========================================")
    print(f"MAIN FUNCTION RETURN: {main_return}")
    print("========================================")
    print(f"GLOBAL MEMORY STATE AT EXIT:")
    print(json.dumps(global_memory, indent=4))
    print("========================================")

if __name__ == "__main__":
    main()