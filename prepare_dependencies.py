#!/usr/bin/env python3
"""Initialize Pomme and apply the tracked SDL target reuse patch."""

from pathlib import Path
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parent
    pomme = root / "extern" / "Pomme"
    patch = root / "patches" / "pomme-reuse-sdl-target.patch"

    subprocess.run(
        ["git", "submodule", "update", "--init", "--recursive", "--", "extern/Pomme"],
        cwd=root,
        check=True,
    )
    apply = ["git", "-C", str(pomme), "apply"]
    forward = subprocess.run(
        apply + ["--check", str(patch)], capture_output=True, text=True
    )
    if forward.returncode == 0:
        subprocess.run(apply + [str(patch)], check=True)
        print("Applied Pomme SDL target reuse patch.")
    elif subprocess.run(
        apply + ["--reverse", "--check", str(patch)], capture_output=True
    ).returncode == 0:
        print("Pomme SDL target reuse patch is already applied.")
    else:
        print(forward.stderr, file=sys.stderr, end="")
        print(
            "Cannot apply the Pomme patch: the checkout is incompatible or has "
            "conflicting edits. Resolve them before building; no patch was applied.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Dependency preparation failed: {error}", file=sys.stderr)
        sys.exit(1)
