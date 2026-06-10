import os
import sys


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    main_py = os.path.join(script_dir, "Main.py")
    args = sys.argv[1:]

    if "--name" not in args:
        args += ["--name", "Player 1"]
    if "--color" not in args:
        args += ["--color", "255,80,80"]

    os.execv(sys.executable, [sys.executable, main_py] + args)


if __name__ == "__main__":
    main()
