#!/usr/bin/env python3
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CORE_TEST_OUTPUT = ROOT / "outputs" / "test.txt"
EFFICIENT_OUTPUT = ROOT / "outputs" / "efficient_summary.txt"
EFFICIENT_BINARY = Path("/tmp/efficient_core")


def read(path):
    return path.read_text(encoding="utf-8")


def require(condition, message):
    if not condition:
        raise SystemExit(f"validation failed: {message}")


def extract_float(label, text):
    match = re.search(rf"{re.escape(label)}([0-9]+(?:\.[0-9]+)?)", text)
    require(match is not None, f"missing {label}")
    return float(match.group(1))


def extract_line(prefix, text):
    for line in text.splitlines():
        if line.startswith(prefix):
            return line
    raise SystemExit(f"validation failed: missing line starting with {prefix}")


def main():
    require(CORE_TEST_OUTPUT.exists(), "outputs/test.txt does not exist")
    require(EFFICIENT_BINARY.exists(), "/tmp/efficient_core does not exist; compile efficient_core first")

    subprocess.run(
        [
            str(EFFICIENT_BINARY),
            "--spins",
            "25",
            "--seed",
            "123456789",
            "--single-thread",
            "--feature-tests",
            "--output",
            str(EFFICIENT_OUTPUT),
        ],
        cwd=ROOT,
        check=True,
    )

    core_text = read(CORE_TEST_OUTPUT)
    efficient_text = read(EFFICIENT_OUTPUT)

    require("Spins=25" in core_text, "core test output does not contain 25-spin run")
    require("Spins=25" in efficient_text, "efficient output does not contain 25-spin run")

    core_rtp = extract_float("RTP:", core_text)
    efficient_rtp = extract_float("RTP:", efficient_text)
    require(abs(core_rtp - efficient_rtp) < 1e-6, f"RTP mismatch: core={core_rtp}, efficient={efficient_rtp}")

    core_var = extract_float("Var:", core_text)
    efficient_var = extract_float("Var:", efficient_text)
    require(abs(core_var - efficient_var) < 1e-3, f"variance mismatch: core={core_var}, efficient={efficient_var}")

    required_core_markers = [
        "FEATURE-SPECIFIC FORCED TEST CASES",
        "GIRDER TEST state change:",
        "EXPHAT TEST payout/state change:",
        "HAT TEST state:",
    ]
    for marker in required_core_markers:
        require(marker in core_text, f"missing marker in outputs/test.txt: {marker}")
        require(marker in efficient_text, f"missing marker in efficient output: {marker}")

    for prefix in [
        "GIRDER TEST state change:",
        "EXPHAT TEST payout/state change:",
        "HAT TEST state:",
    ]:
        core_line = extract_line(prefix, core_text)
        efficient_line = extract_line(prefix, efficient_text)
        require(core_line == efficient_line, f"feature line mismatch for {prefix}\ncore={core_line}\nefficient={efficient_line}")

    print("validation passed: efficient_core summary and feature markers match outputs/test.txt")


if __name__ == "__main__":
    main()
