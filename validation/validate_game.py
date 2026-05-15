#!/usr/bin/env python3
from pathlib import Path
import csv
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
GAME_BIN = Path("/tmp/game")
RTP_REPORT = ROOT / "outputs" / "rtp_report.txt"
SYMBOL_REPORT = ROOT / "outputs" / "symbol_distribution.csv"
TEST_LOG = ROOT / "outputs" / "test.txt"


def run(command):
    return subprocess.run(
        command,
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def extract_number(pattern, text):
    match = re.search(pattern, text)
    if not match:
        raise AssertionError(f"missing pattern: {pattern}")
    return float(match.group(1))


def main():
    if not TEST_LOG.exists():
        raise AssertionError("outputs/test.txt is required; run core/core_test.cpp first")

    run(["g++", "-O3", "-std=c++17", "-Wall", "-Wextra", "game.cpp", "-o", str(GAME_BIN)])
    run([
        str(GAME_BIN),
        "--spins", "25",
        "--seed", "123456789",
        "--single-thread",
        "--rtp-output", str(RTP_REPORT),
        "--symbol-output", str(SYMBOL_REPORT),
    ])

    test_text = TEST_LOG.read_text()
    report_text = RTP_REPORT.read_text()

    expected_rtp = extract_number(r"RTP:([0-9.]+)", test_text)
    expected_var = extract_number(r"Var:([0-9.]+)", test_text)
    expected_free_trigger = extract_number(r"Free Games:([0-9.]+)", test_text)

    actual_rtp = extract_number(r"RTP=([0-9.]+)", report_text)
    actual_var = extract_number(r"Variance=([0-9.]+)", report_text)
    actual_free_trigger = extract_number(r"FreeGameTrigger=([0-9.]+)", report_text)

    if abs(expected_rtp - actual_rtp) > 1e-6:
        raise AssertionError(f"RTP mismatch: expected {expected_rtp}, got {actual_rtp}")
    if abs(expected_var - actual_var) > 1e-3:
        raise AssertionError(f"Variance mismatch: expected {expected_var}, got {actual_var}")
    if abs(expected_free_trigger - actual_free_trigger) > 1e-6:
        raise AssertionError(
            f"FreeGameTrigger mismatch: expected {expected_free_trigger}, got {actual_free_trigger}"
        )

    with SYMBOL_REPORT.open(newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader)
    expected_header = ["Symbol", "Occurrence", "Win", "Mode", "Hits", "RTP"]
    if header != expected_header:
        raise AssertionError(f"CSV header mismatch: expected {expected_header}, got {header}")

    print("validation passed: game.cpp reports match outputs/test.txt and CSV schema")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, subprocess.CalledProcessError) as exc:
        print(f"validation failed: {exc}", file=sys.stderr)
        sys.exit(1)
