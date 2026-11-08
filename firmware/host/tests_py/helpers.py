"""Shared helpers for the Python host tests."""
import importlib.util
import pathlib
import subprocess

HOST = pathlib.Path(__file__).resolve().parent.parent
BUILD = HOST / "build"


def load_analyzer():
    spec = importlib.util.spec_from_file_location("analyze_log", HOST / "analyze_log.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_binary(name, *args):
    exe = BUILD / name
    if not exe.exists():
        raise FileNotFoundError("%s is not built. Run `make test-tools` in firmware/host." % exe)
    return subprocess.run([str(exe)] + [str(a) for a in args], capture_output=True, text=True, timeout=120)


def sample_log(minutes, mode="clean"):
    """A synthetic log made with the firmware's real formatters (not real hardware output)."""
    result = run_binary("stats_sample", minutes, mode)
    assert result.returncode == 0, result.stderr
    return result.stdout


def hw_check(scenario, loops=3):
    """Output of the real hw_check.cpp running against a simulated board."""
    result = run_binary("hw_check_sim", scenario, loops)
    assert result.returncode == 0, result.stderr
    return result.stdout


def read_pbm(path):
    tokens = pathlib.Path(path).read_text().split()
    assert tokens[0] == "P1"
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    assert len(bits) == w * h
    return w, h, [[int(bits[y * w + x]) for x in range(w)] for y in range(h)]
