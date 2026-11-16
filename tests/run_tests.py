"""Test runner for ./coloring (run from the project root: make test).

1. Fixed cases (tests/cases): first output line must match the .out file,
   and the printed coloring must pass the validity checker. Every case runs
   in auto, --dp (when n <= 20) and --bnb mode, and all modes must print the
   exact same output. A .exact file, if present, must match stdout exactly.
2. Invalid inputs (tests/invalid): must exit with code 2 and print "error:".
3. Brute force: random tiny graphs (n <= 7). Both solvers must print the
   lexicographically smallest optimal coloring found by trying every one.
4. Cross-check: random graphs, 8 <= n <= 16. DP and B&B must agree on the
   spread, and both colorings must be valid.
"""
import itertools
import os
import random
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "coloring")
CASES = os.path.join(ROOT, "tests", "cases")
INVALID = os.path.join(ROOT, "tests", "invalid")
NONE = "no such a sequence exists"
DP_MAX_N = 20

failures = 0
passed = 0


def check(cond, label, detail=""):
    global failures, passed
    if cond:
        passed += 1
    else:
        failures += 1
        print(f"FAIL {label}" + (f"\n     {detail}" if detail else ""))


def load(path):
    """Reads k and the graph (adjacency sets, symmetrized like the program)."""
    tok = open(path, encoding="utf-8-sig").read().split()
    k, n = int(tok[0]), int(tok[1])
    vals = list(map(int, tok[2:2 + n * n]))
    adj = [set() for _ in range(n)]
    for i in range(n):
        for j in range(n):
            if vals[i * n + j]:
                adj[i].add(j)
                adj[j].add(i)
    return k, n, adj


def run(path, mode=None):
    args = [BIN, path] + ([mode] if mode else [])
    p = subprocess.run(args, capture_output=True, text=True, timeout=120)
    return p.returncode, p.stdout, p.stderr


def validate(out, k, n, adj):
    """Returns (ok, message, spread or None). Checks the printed coloring."""
    lines = out.strip().split("\n")
    if lines[0] == NONE:
        return len(lines) == 1, "extra lines after 'none'", None
    seq = list(map(int, lines[0].strip("()").split(",")))
    if len(seq) != k:
        return False, f"sequence has {len(seq)} entries, k = {k}", None
    if seq != sorted(seq, reverse=True) or min(seq) < 1:
        return False, f"sequence not non-increasing / has zero: {seq}", None
    if len(lines) != k + 1:
        return False, f"expected {k} color lines, got {len(lines) - 1}", None
    seen = set()
    for size, line in zip(seq, lines[1:]):
        name, _, vs = line.partition(":")
        verts = [int(x) - 1 for x in vs.split(",")]
        if len(verts) != size:
            return False, f"line '{line}' has {len(verts)} vertices, expected {size}", None
        if verts != sorted(verts):
            return False, f"vertices not ascending in '{line}'", None
        for a in verts:
            if adj[a] & set(verts):
                return False, f"adjacent vertices share color in '{line}'", None
        seen.update(verts)
    if seen != set(range(n)) or sum(seq) != n:
        return False, "not every vertex colored exactly once", None
    return True, "", seq[0] - seq[-1]


def write_graph(k, n, adj, path):
    with open(path, "w") as f:
        f.write(f"{k}\n{n}\n")
        for i in range(n):
            f.write(" ".join("1" if j in adj[i] else "0" for j in range(n)) + "\n")


def random_connected(n, p, rng):
    adj = [set() for _ in range(n)]
    for v in range(1, n):                     # random spanning tree
        u = rng.randrange(v)
        adj[u].add(v)
        adj[v].add(u)
    for i in range(n):
        for j in range(i + 1, n):
            if rng.random() < p:
                adj[i].add(j)
                adj[j].add(i)
    return adj


def random_bipartite(n, p, rng):
    """Connected bipartite graph with uneven sides (vertex 0 and vertex a are hubs)."""
    a = rng.randint(1, n - 1)
    adj = [set() for _ in range(n)]
    for i in range(a):
        for j in range(a, n):
            if i == 0 or j == a or rng.random() < p:
                adj[i].add(j)
                adj[j].add(i)
    return adj


def random_hub(n, p, rng):
    """Random tree plus one hub joined to many vertices (star-like)."""
    adj = random_connected(n, 0.0, rng)
    h = rng.randrange(n)
    for v in range(n):
        if v != h and rng.random() < p:
            adj[h].add(v)
            adj[v].add(h)
    return adj


def random_graph(n, rng):
    """Mix of plain random, uneven bipartite, and star-like graphs."""
    kind = rng.randrange(3)
    if kind == 0:
        return random_connected(n, rng.choice([0.1, 0.3, 0.6]), rng)
    if kind == 1:
        return random_bipartite(n, rng.choice([0.1, 0.3, 0.6]), rng)
    return random_hub(n, 0.6, rng)


def brute_best(k, n, adj):
    """Tries all k^n colorings. Returns (optimal spread, the classes of the
    lexicographically smallest optimal coloring in first-appearance form),
    or (None, None) if no coloring uses all k colors."""
    best, best_col = None, None
    for col in itertools.product(range(k), repeat=n):   # lexicographic order
        if any(col[a] == col[b] for a in range(n) for b in adj[a]):
            continue
        sizes = [col.count(c) for c in range(k)]
        if min(sizes) == 0:
            continue
        s = max(sizes) - min(sizes)
        if best is None or s < best:
            best, best_col = s, col
    if best is None:
        return None, None
    # renumber by first appearance; lexicographic order on such colorings
    # is the same as on the raw tuples once restricted to that form
    relabel, canon = {}, []
    for c in best_col:
        relabel.setdefault(c, len(relabel))
        canon.append(relabel[c])
    return best, canon


def printed_classes(out):
    """Set of frozensets of 1-indexed vertices from the color lines."""
    lines = out.strip().split("\n")[1:]
    return {frozenset(int(x) for x in l.partition(":")[2].split(",")) for l in lines}


def modes_for(n):
    return [None, "--bnb"] + (["--dp"] if n <= DP_MAX_N else [])


def fixed_cases():
    for fname in sorted(os.listdir(CASES)):
        if not fname.endswith(".in"):
            continue
        path = os.path.join(CASES, fname)
        expected = open(path[:-3] + ".out").read().strip()
        k, n, adj = load(path)
        exact = path[:-3] + ".exact"
        outputs = set()
        for mode in modes_for(n):
            label = f"{fname} [{mode or 'auto'}]"
            code, out, err = run(path, mode)
            outputs.add(out)
            if os.path.exists(exact):
                want = open(exact).read()
                check(out == want, label + " exact output", f"got:\n{out}\nwant:\n{want}")
            check(code == 0, label, f"exit {code}: {err.strip()}")
            first = out.strip().split("\n")[0] if out.strip() else ""
            check(first == expected, label, f"got '{first}', expected '{expected}'")
            ok, msg, _ = validate(out, k, n, adj)
            check(ok, label + " validity", msg)
        check(len(outputs) == 1, f"{fname} same output in every mode")


def invalid_cases():
    for fname in sorted(os.listdir(INVALID)):
        path = os.path.join(INVALID, fname)
        code, out, err = run(path)
        check(code == 2 and err.startswith("error:"), f"invalid/{fname}",
              f"exit {code}, stderr '{err.strip()}'")
    code, _, err = run(os.path.join(INVALID, "does_not_exist.in"))
    check(code == 2 and "cannot open" in err, "missing file", f"exit {code}")


def brute_force(rng, tmp):
    for trial in range(150):
        n = rng.randint(1, 7)
        k = rng.randint(1, min(n + 1, 4))
        adj = random_graph(n, rng) if n > 1 else [set()]
        path = os.path.join(tmp, "bf.in")
        write_graph(k, n, adj, path)
        truth, canon = brute_best(k, n, adj)
        for mode in ("--dp", "--bnb"):
            _, out, _ = run(path, mode)
            ok, msg, spread = validate(out, k, n, adj)
            label = f"brute n={n} k={k} trial={trial} [{mode}]"
            check(ok, label, msg)
            check(spread == truth, label, f"spread {spread}, optimal {truth}")
            if canon is not None and ok:
                want = {frozenset(v + 1 for v in range(n) if canon[v] == c)
                        for c in range(k)}
                check(printed_classes(out) == want, label + " lexicographic",
                      f"got {sorted(map(sorted, printed_classes(out)))}, "
                      f"want {sorted(map(sorted, want))}")


def cross_check(rng, tmp):
    for trial in range(150):
        n = rng.randint(8, 16)
        k = rng.randint(2, 6)
        adj = random_graph(n, rng)
        path = os.path.join(tmp, "cc.in")
        write_graph(k, n, adj, path)
        outs = {}
        for mode in ("--dp", "--bnb"):
            _, outs[mode], _ = run(path, mode)
            ok, msg, _ = validate(outs[mode], k, n, adj)
            check(ok, f"cross n={n} k={k} trial={trial} [{mode}]", msg)
        check(outs["--dp"] == outs["--bnb"], f"cross n={n} k={k} trial={trial}",
              f"dp:\n{outs['--dp']}bnb:\n{outs['--bnb']}")


def main():
    if not os.path.exists(BIN):
        sys.exit("build first: make")
    rng = random.Random(421)
    with tempfile.TemporaryDirectory() as tmp:
        fixed_cases()
        invalid_cases()
        brute_force(rng, tmp)
        cross_check(rng, tmp)
    print(f"\n{passed} passed, {failures} failed")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
