"""Run with: xvfb-run -a python3 tests/icon_smoke.py build/easy_git"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

with tempfile.TemporaryDirectory(prefix="easy-git-icon-") as directory:
    root = pathlib.Path(directory)
    binary = shutil.copy2(sys.argv[1], root / "easy_git")
    process = subprocess.Popen([binary, "--config", str(root / "settings")], cwd=root)
    try:
        for _ in range(100):
            assert process.poll() is None, "Application exited before creating its window"
            result = subprocess.run(
                ["xprop", "-name", "Easy Git - repository workspace", "-f", "_NET_WM_ICON", "32c", "_NET_WM_ICON", "WM_CLASS"],
                capture_output=True, text=True)
            if result.returncode == 0 and "_NET_WM_ICON(CARDINAL) = " in result.stdout:
                break
            time.sleep(0.05)
        else:
            raise AssertionError("Window icon missing")
        pixels = [int(value) for value in re.findall(r"\d+", result.stdout.split(" = ", 1)[1].splitlines()[0])]
        assert pixels[:2] == [64, 64] and len(pixels) == 64 * 64 + 2
        assert len(set(pixels[2:])) > 2, "Icon is blank"
        assert 'WM_CLASS(STRING) = "easy_git", "easy_git"' in result.stdout
        print("Embedded icon and desktop window class verified with a relocated executable")
    finally:
        process.terminate()
        process.wait(timeout=10)
