"""Runtime measurements for the report (run: python3 tests/bench.py).

Prints the median time for DP and B&B on random connected graphs of growing
size n, so the measured growth can be compared with the analysis.
"""
import os
import random
import statistics
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run_tests import BIN, random_connected, write_graph  # noqa: E402

TIMEOUT = 30


def timed(path, mode):
    t = time.perf_counter()
    try:
        subprocess.run([BIN, path, mode], capture_output=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return None
    return time.perf_counter() - t


def bench(mode, sizes, k, p, reps=3):
    rng = random.Random(7)
    print(f"\n{mode}  k={k}  edge prob={p}")
    print(f"{'n':>4} {'median s':>10}")
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "b.in")
        for n in sizes:
            times = []
            for _ in range(reps):
                write_graph(k, n, random_connected(n, p, rng), path)
                times.append(timed(path, mode))
            if None in times:
                print(f"{n:>4} {'> ' + str(TIMEOUT):>10}")
                break
            print(f"{n:>4} {statistics.median(times):>10.4f}")


if __name__ == "__main__":
    bench("--dp", range(10, 21, 2), k=3, p=0.3)
    bench("--bnb", range(10, 61, 5), k=3, p=0.1)
    bench("--bnb", range(10, 61, 5), k=4, p=0.3)
