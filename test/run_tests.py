import os
import subprocess
import sys
from pathlib import Path


def main():
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent

    os.chdir(project_root)

    output_dir = script_dir / "output"
    output_dir.mkdir(parents=True, exist_ok=True)

    zasm_files = sorted(script_dir.glob("*.zasm"))

    if not zasm_files:
        print("[!] No .zasm files found in test directory.")
        sys.exit(0)

    print(f"[*] Found {len(zasm_files)} .zasm file(s). Running tests...\n")

    for zasm_file in zasm_files:
        rel_path = zasm_file.relative_to(project_root)
        output_file = output_dir / f"{zasm_file.stem}.dump"

        cmd = ["xmake", "run", "Zenith", str(rel_path)]

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, check=False
            )

            with open(output_file, "w", encoding="utf-8") as f:
                if result.stdout:
                    f.write(result.stdout)
                if result.stderr:
                    f.write("\n--- STDERR ---\n")
                    f.write(result.stderr)

            print(f"  [+] {rel_path} -> {output_file.relative_to(project_root)}")

        except Exception as e:
            print(f"  [!] Failed to execute {rel_path}: {e}")

    print("\n[*] All execution dumps generated successfully.")


if __name__ == "__main__":
    main()