"""Writes the test inputs in tests/cases and tests/invalid.

Each valid case gets a .in file and a .out file. The .out file holds only the
expected first line: the sequence or "no such a sequence exists". Every
expected answer below was worked out by hand (the reasoning is in the comment
next to it). Class membership is not fixed, because several optimal colorings
can exist; run_tests.py checks it with a validity checker instead.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
CASES = os.path.join(HERE, "cases")
INVALID = os.path.join(HERE, "invalid")
NONE = "no such a sequence exists"


def matrix(n, edges):
    m = [[0] * n for _ in range(n)]
    for a, b in edges:
        m[a][b] = m[b][a] = 1
    return m


def text(k, m):
    return f"{k}\n{len(m)}\n" + "\n".join(" ".join(map(str, r)) for r in m) + "\n"


def path(n):
    return [(i, i + 1) for i in range(n - 1)]


def cycle(n):
    return path(n) + [(n - 1, 0)]


def complete(n):
    return [(i, j) for i in range(n) for j in range(i + 1, n)]


def complete_bipartite(a, b):
    return [(i, a + j) for i in range(a) for j in range(b)]


def star(leaves):
    return [(0, i) for i in range(1, leaves + 1)]


def grid(r, c):
    e = []
    for i in range(r):
        for j in range(c):
            v = i * c + j
            if j + 1 < c:
                e.append((v, v + 1))
            if i + 1 < r:
                e.append((v, v + c))
    return e


def tree8():
    # 8-vertex tree (2-colorable):
    # 1-2, 2-3, 3-4, 4-5, 3-6, 3-7, 3-8  (0-indexed below)
    return [(0, 1), (1, 2), (2, 3), (3, 4), (2, 5), (2, 6), (2, 7)]


def graph20():
    # 20-vertex reference graph. Vertices A..T are
    # numbered 1..20 in this order; the 4-coloring
    # (Red A H L O P, Yellow B G J M R, Blue C E I Q S, Green D F K N T)
    # is proper on these 43 edges.
    names = "ABCDEFGHIJKLMNOPQRST"
    pairs = ("AB AF AG BC BD BE CD CL DE DH DM EH EF EJ FG FI FJ GI GK HM HN "
             "IJ IK IO JN JO JQ KO KR LM LS MN MP MS NP OQ OR PQ PS QR QT RT ST")
    return [(names.index(e[0]), names.index(e[1])) for e in pairs.split()]


def petersen():
    outer = [(i, (i + 1) % 5) for i in range(5)]
    spokes = [(i, i + 5) for i in range(5)]
    inner = [(5 + i, 5 + (i + 2) % 5) for i in range(5)]
    return outer + spokes + inner


# name: (k, n, edges, expected first line)
VALID = {
    # K_{3,5}: the only 2-coloring is the two sides -> (5,3). With 3 colors
    # the 5-side splits into 3+2 -> (3,3,2). Matches the expected answer.
    "k35_k2": (2, 8, complete_bipartite(3, 5), "(5,3)"),
    "k35_k3": (3, 8, complete_bipartite(3, 5), "(3,3,2)"),
    # single vertex
    # tree8: k=2 -> (5,3) and k=3 -> (3,3,2).
    # k=1 impossible (has edges); k=4: vertex 3 (degree 5) can share a class
    # only with 1 or 5, and 2,4,6,7,8 need to fill the rest -> (2,2,2,2).
    "tree8_k1": (1, 8, tree8(), NONE),
    "tree8_k2": (2, 8, tree8(), "(5,3)"),
    "tree8_k3": (3, 8, tree8(), "(3,3,2)"),
    "tree8_k4": (4, 8, tree8(), "(2,2,2,2)"),
    # graph20: k=4 -> (5,5,5,5). It has triangles (B C D),
    # so k=2 is impossible. k=3 is also impossible:
    # O is joined to all of the 5-cycle I-J-Q-R-K (an odd wheel), and an odd
    # wheel needs 4 colors.
    "graph20_k2": (2, 20, graph20(), NONE),
    "graph20_k3": (3, 20, graph20(), NONE),
    "graph20_k4": (4, 20, graph20(), "(5,5,5,5)"),
    # disconnected input: the input is expected to be connected, but we warn and solve
    "disconnected_k2": (2, 4, [(0, 1), (2, 3)], "(2,2)"),
    "single_k1": (1, 1, [], "(1)"),
    "single_k2": (2, 1, [], NONE),                       # k > n
    # K2 needs 2 colors
    "edge_k1": (1, 2, [(0, 1)], NONE),
    "edge_k2": (2, 2, [(0, 1)], "(1,1)"),
    # K4: needs 4 colors; k = n gives all singletons
    "k4_k3": (3, 4, complete(4), NONE),
    "k4_k4": (4, 4, complete(4), "(1,1,1,1)"),
    "k4_k5": (5, 4, complete(4), NONE),                  # k > n
    # path P5: 2-coloring is forced (3,2); with 3 colors (2,2,1)
    "path5_k2": (2, 5, path(5), "(3,2)"),
    "path5_k3": (3, 5, path(5), "(2,2,1)"),
    # star with 4 leaves: center alone, leaves together -> (4,1);
    # with 3 colors the leaves split 2+2 -> (2,2,1)
    "star4_k2": (2, 5, star(4), "(4,1)"),
    "star4_k3": (3, 5, star(4), "(2,2,1)"),
    # even cycle is bipartite and balanced; odd cycle is not 2-colorable
    "c6_k2": (2, 6, cycle(6), "(3,3)"),
    "c5_k2": (2, 5, cycle(5), NONE),
    "c5_k3": (3, 5, cycle(5), "(2,2,1)"),
    # Petersen: chromatic number 3, max independent set 4 -> (4,3,3)
    "petersen_k2": (2, 10, petersen(), NONE),
    "petersen_k3": (3, 10, petersen(), "(4,3,3)"),
    # larger graphs that auto mode sends to B&B (n > 16)
    "grid5x6_k2": (2, 30, grid(5, 6), "(15,15)"),        # bipartite, balanced
    "c30_k3": (3, 30, cycle(30), "(10,10,10)"),           # repeat colors 1,2,3
    "c31_k2": (2, 31, cycle(31), NONE),                   # odd cycle
    "k20_k19": (19, 20, complete(20), NONE),              # clique needs 20
    # star with 39 leaves: center is alone, leaves split 20+19 -> (20,19,1)
    "star39_k3": (3, 40, star(39), "(20,19,1)"),
    # K_{10,30}: each class stays in one side; best is 30 -> 15+15 -> (15,15,10)
    "k10_30_k3": (3, 40, complete_bipartite(10, 30), "(15,15,10)"),
    "path64_k2":(2, 64, path(64), "(32,32)"),            # n = 64 edge case
    "path64_k64": (64, 64, path(64), "(" + ",".join(["1"] * 64) + ")"),
}

SAMPLE6 = """0 1 1 0 0 0
1 0 1 1 0 0
1 1 0 0 1 1
0 1 0 0 0 1
0 0 1 0 0 1
0 0 1 1 1 0
"""

# same graph with one entry missing (row 6, column 3): must still be read
# as edge 3-6, with a warning on stderr
SAMPLE6_ONE_SIDED = SAMPLE6.replace("0 0 1 1 1 0", "0 0 0 1 1 0")

# name: file content (each must exit with code 2)
INVALID_FILES = {
    "empty": "",
    "k_zero": "0\n2\n0 1\n1 0\n",
    "k_negative": "-1\n2\n0 1\n1 0\n",
    "k_not_int": "abc\n2\n0 1\n1 0\n",
    "n_missing": "2\n",
    "n_zero": "2\n0\n",
    "n_too_big": "2\n65\n",
    "too_few_entries": "2\n3\n0 1 0\n1 0 1\n0 1\n",
    "extra_entries": "2\n2\n0 1\n1 0\n1\n",
    "entry_not_01": "2\n2\n0 2\n2 0\n",
    "entries_no_space": "2\n2\n01\n10\n",
    "self_loop": "2\n2\n1 1\n1 0\n",
}


def main():
    os.makedirs(CASES, exist_ok=True)
    os.makedirs(INVALID, exist_ok=True)

    # reference sample, typed exactly as printed
    for k, exp in ((3, "(2,2,2)"), (2, NONE)):
        with open(os.path.join(CASES, f"sample6_k{k}.in"), "w") as f:
            f.write(f"{k}\n6\n{SAMPLE6}")
        with open(os.path.join(CASES, f"sample6_k{k}.out"), "w") as f:
            f.write(exp + "\n")

    with open(os.path.join(CASES, "one_sided_k3.in"), "w") as f:
        f.write(f"3\n6\n{SAMPLE6_ONE_SIDED}")
    with open(os.path.join(CASES, "one_sided_k3.out"), "w") as f:
        f.write("(2,2,2)\n")

    for name, (k, n, edges, exp) in VALID.items():
        with open(os.path.join(CASES, name + ".in"), "w") as f:
            f.write(text(k, matrix(n, edges)))
        with open(os.path.join(CASES, name + ".out"), "w") as f:
            f.write(exp + "\n")

    # exact expected output for the reference sample
    with open(os.path.join(CASES, "sample6_k3.exact"), "w") as f:
        f.write("(2,2,2)\nBlue   : 2, 6\nRed    : 3, 4\nYellow : 1, 5\n")

    # same sample saved with a UTF-8 byte order mark and Windows line endings
    with open(os.path.join(CASES, "sample6_k3_bom_crlf.in"), "wb") as f:
        f.write(b"\xef\xbb\xbf" + f"3\n6\n{SAMPLE6}".replace("\n", "\r\n").encode())
    with open(os.path.join(CASES, "sample6_k3_bom_crlf.out"), "w") as f:
        f.write("(2,2,2)\n")

    for name, content in INVALID_FILES.items():
        with open(os.path.join(INVALID, name + ".in"), "w") as f:
            f.write(content)


if __name__ == "__main__":
    main()
