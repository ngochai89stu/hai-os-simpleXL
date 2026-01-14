#!/usr/bin/env python3
"""Simple stress-test runner for hai-os-simplexl.

This script iterates over test binaries tagged with `@stress`, flashes them
(if HW connected) or runs them in QEMU (future), captures UART output and
writes a summary JUnit XML for CI systems.

Phase 4 improvements:
• Discovers Unity tests with `idf.py list-tests`
• Filters tests that contain `@stress` tag in the test name or comment
• Also filters tests matching Phase 3 Test Matrix patterns (BOOT-*, WIFI-*, SD-*, AUD-*, NET-*, UI-*)
• Executes them via `idf.py test -q` (quiet) one by one so that a single
  failure doesn't abort the entire matrix
• Aggregates results into a simple summary printed at the end
• Exports metrics to Prometheus format after test run

NOTE: Requires ESP-IDF tools in PATH (idf.py). For CI this is already handled
by `setup-esp-idf` action.
"""
from __future__ import annotations
import subprocess
import re
import json
import os
import sys
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Tuple

RE_STRESS = re.compile(r"@stress", re.IGNORECASE)
# Phase 4: Match Phase 3 Test Matrix test IDs
RE_TEST_MATRIX = re.compile(r"(BOOT|WIFI|SD|AUD|NET|UI)-\d+", re.IGNORECASE)
# Phase 4: Match stress-related test names
RE_STRESS_PATTERNS = [
    re.compile(r"stress", re.IGNORECASE),
    re.compile(r"performance", re.IGNORECASE),
    re.compile(r"load", re.IGNORECASE),
    re.compile(r"under.*load", re.IGNORECASE),
]

PROJECT_ROOT = Path(__file__).resolve().parent.parent
UNIT_TEST_DIR = PROJECT_ROOT / "test" / "unit_test"
METRICS_EXPORT_DIR = PROJECT_ROOT / "test" / "metrics"


def list_unity_tests() -> list[str]:
    """Return list of test names as reported by `idf.py list-tests`."""
    try:
        result = subprocess.run(
            ["idf.py", "list-tests"], cwd=UNIT_TEST_DIR, check=True,
            capture_output=True, text=True
        )
    except (subprocess.CalledProcessError, FileNotFoundError) as err:
        print(f"ERROR: Cannot list tests: {err}")
        return []

    tests = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        tests.append(line)
    return tests


def is_stress_test(test_name: str) -> bool:
    """Return True if test name contains @stress tag or matches stress patterns.
    
    Phase 4: Also matches Phase 3 Test Matrix test IDs and stress-related patterns.
    """
    # Check for @stress tag
    if RE_STRESS.search(test_name):
        return True
    
    # Check for Phase 3 Test Matrix test IDs
    if RE_TEST_MATRIX.search(test_name):
        return True
    
    # Check for stress-related patterns
    for pattern in RE_STRESS_PATTERNS:
        if pattern.search(test_name):
            return True
    
    return False


def run_test(test_name: str) -> Tuple[bool, Dict[str, any]]:
    """Run a single Unity test by name. Return (passed, metrics_dict).
    
    Phase 4: Returns metrics info if available.
    """
    cmd = ["idf.py", "test", f"--test-case={test_name}", "-q"]
    print(f"\n[RUN] {test_name}")
    
    start_time = datetime.now()
    try:
        result = subprocess.run(
            cmd, cwd=UNIT_TEST_DIR, check=True,
            capture_output=True, text=True
        )
        elapsed = (datetime.now() - start_time).total_seconds()
        print(f"[PASS] {test_name} ({elapsed:.2f}s)")
        
        # Phase 4: Try to extract metrics from output (if available)
        metrics = extract_metrics_from_output(result.stdout)
        return True, metrics
    except subprocess.CalledProcessError as e:
        elapsed = (datetime.now() - start_time).total_seconds()
        print(f"[FAIL] {test_name} ({elapsed:.2f}s)")
        if e.stdout:
            print(f"STDOUT: {e.stdout[:500]}")
        if e.stderr:
            print(f"STDERR: {e.stderr[:500]}")
        return False, {}


def extract_metrics_from_output(output: str) -> Dict[str, any]:
    """Extract metrics from test output if available.
    
    Phase 4: Looks for metrics patterns in test output.
    """
    metrics = {}
    
    # Look for UI render time
    match = re.search(r"ui_render.*?(\d+)\s*ms", output, re.IGNORECASE)
    if match:
        metrics["ui_render_ms"] = int(match.group(1))
    
    # Look for heap free
    match = re.search(r"heap.*?free.*?(\d+)", output, re.IGNORECASE)
    if match:
        metrics["heap_free_bytes"] = int(match.group(1))
    
    # Look for event counts
    match = re.search(r"events.*?(\d+)", output, re.IGNORECASE)
    if match:
        metrics["events_total"] = int(match.group(1))
    
    return metrics


def export_metrics_summary(results: List[Tuple[str, bool, Dict]], output_file: str):
    """Export test results and metrics summary to file.
    
    Phase 4: Creates a summary file with test results and metrics.
    """
    METRICS_EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = METRICS_EXPORT_DIR / output_file
    
    with open(output_path, "w") as f:
        f.write("# Stress Test Results Summary\n\n")
        f.write(f"Generated: {datetime.now().isoformat()}\n\n")
        
        passed = sum(1 for _, passed, _ in results if passed)
        total = len(results)
        f.write(f"## Summary\n")
        f.write(f"- Passed: {passed}/{total}\n")
        f.write(f"- Failed: {total - passed}/{total}\n\n")
        
        f.write("## Test Results\n\n")
        for test_name, passed, metrics in results:
            status = "PASS" if passed else "FAIL"
            f.write(f"### {test_name} - {status}\n")
            if metrics:
                f.write("Metrics:\n")
                for key, value in metrics.items():
                    f.write(f"- {key}: {value}\n")
            f.write("\n")
    
    print(f"\n[METRICS] Summary exported to {output_path}")


def main() -> int:
    """Main entry point for stress test runner.
    
    Phase 4: Enhanced with metrics export and better reporting.
    """
    print("=" * 50)
    print("Phase 4: Stress Test Runner")
    print("=" * 50)
    
    tests = list_unity_tests()
    if not tests:
        print("ERROR: No tests found. Is idf.py available?")
        return 1
    
    print(f"Found {len(tests)} total tests")
    
    stress_tests = [t for t in tests if is_stress_test(t)]
    if not stress_tests:
        print("WARNING: No stress tests found – skipping.")
        print("HINT: Tag tests with @stress or use stress/performance/load in name")
        return 0

    print(f"Running {len(stress_tests)} stress tests...")
    print(f"Tests: {', '.join(stress_tests)}")
    
    results: List[Tuple[str, bool, Dict]] = []
    passed = 0
    
    for t in stress_tests:
        test_passed, metrics = run_test(t)
        results.append((t, test_passed, metrics))
        if test_passed:
            passed += 1

    total = len(stress_tests)
    print("\n" + "=" * 50)
    print("Stress-Test Summary")
    print("=" * 50)
    print(f"Passed: {passed}/{total} tests")
    print(f"Failed: {total - passed}/{total} tests")
    
    # Phase 4: Export metrics summary
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    export_metrics_summary(results, f"stress_test_results_{timestamp}.md")
    
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
