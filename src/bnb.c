/* bnb.c - backtracking with branch-and-bound
 *
 * Colors vertices one at a time (highest degree first). At every node it
 * computes a lower bound on the spread of any completion and cuts the branch
 * if that bound cannot beat the best spread found so far. Stops early when
 * the spread reaches the overall lower bound (0 or 1).
 * Worst case: O(k^n) tree nodes x O(k^2 + k log n) work per node.
 *
 * The same search also gives the canonical answer (canonicalize): vertices
 * in order 1..n, colors tried 0, 1, 2, ..., only spread <= optimum allowed.
 * The first leaf reached is the lexicographically smallest optimal coloring. */
#include <limits.h>
#include <string.h>

#include "graph.h"

static const Graph *G;
static int N, K;
static int order[MAX_N];          /* vertices sorted by degree, highest first */
static uint64_t classMask[MAX_N]; /* classMask[c]: vertices with color c */
static uint64_t nbrMask[MAX_N];   /* nbrMask[c]: union of neighbors of class c */
static uint64_t uncolored;        /* vertices not yet colored */
static int classSize[MAX_N];
static int color[MAX_N];
static int used;                  /* colors 0..used-1 are in use */
static int best;                  /* best spread so far; N + 1 means none found */
static int bestColor[MAX_N];
static int target;                /* spread lower bound; reaching it ends the search */
static int floorAvg, ceilAvg;     /* floor(n/k), ceil(n/k) */
static int naturalOrder;          /* 1: vertex order 1..n and colors 0,1,2,... */
static long long nodes, budget;   /* node count and limit (canonical pass only) */
static int aborted;               /* set when nodes > budget */

#ifndef CANON_BUDGET
#define CANON_BUDGET 20000000LL   /* about a few seconds of search */
#endif

/* Smallest M >= from such that sum over c of min(cap[c], M) >= N.
 * Final sizes are <= cap[c] and add up to N, so the final max is >= M.
 * Binary search over M: O(k log n). */
static int water_fill(const int *cap, int from) {
    int lo = from, hi = N;
    while (lo < hi) {
        int mid = (lo + hi) / 2, sum = 0;
        for (int c = 0; c < K; c++) sum += cap[c] < mid ? cap[c] : mid;
        if (sum >= N) hi = mid;
        else lo = mid + 1;
    }
    return lo;
}

static void search(int idx, int curMax) {
    int remaining = N - idx;

    if (++nodes > budget) {
        aborted = 1;
        return;
    }

    /* Prune 1: not enough vertices left to give every unused color one vertex. */
    if (remaining < K - used) return;

    /* cap[c] = most vertices class c can end with: its size plus the
     * uncolored vertices with no neighbor in it. An unused color can take
     * any of the remaining vertices. O(k) mask ops. */
    int cap[MAX_N], capSum = 0;
    uint64_t fits = 0; /* uncolored vertices that fit at least one used class */
    for (int c = 0; c < K; c++) {
        if (c < used) {
            uint64_t avail = uncolored & ~nbrMask[c];
            cap[c] = classSize[c] + __builtin_popcountll(avail);
            fits |= avail;
        } else {
            cap[c] = remaining;
        }
        capSum += cap[c];
    }

    /* Prune 2 (forward check): with all k colors in use, an uncolored vertex
     * that clashes with every class can never be colored. */
    if (used == K && fits != uncolored) return;
    if (capSum < N) return;

    /* Prune 3 (bound): spread of any completion >= maxLB - minUB.
     *   final min <= floor(n/k) and <= every cap[c]
     *   final max >= curMax, >= ceil(n/k), and >= the water-fill level
     * At a leaf every cap equals the class size, so this is the exact spread. */
    int minUB = floorAvg;
    for (int c = 0; c < K; c++)
        if (cap[c] < minUB) minUB = cap[c];
    int maxLB = water_fill(cap, curMax > ceilAvg ? curMax : ceilAvg);
    if (maxLB - minUB >= best) return;

    if (idx == N) {
        best = maxLB - minUB;
        memcpy(bestColor, color, sizeof color);
        return;
    }

    int v = order[idx];

    /* Candidate colors. Symmetry breaking: the colors already used plus at
     * most one new one; color names are interchangeable, so this skips the
     * k! relabelings of each coloring. */
    int cand[MAX_N], nc = 0;
    int limit = used < K ? used : K - 1;
    for (int c = 0; c <= limit; c++)
        if (!(G->adj[v] & classMask[c])) cand[nc++] = c;  /* O(1) clash check */

    /* Value ordering: smallest class first, so balanced colorings (and a
     * tight best) are found early. Insertion sort, O(k^2). The canonical
     * pass keeps colors in index order instead. */
    for (int i = 1; i < nc && !naturalOrder; i++) {
        int c = cand[i], j = i - 1;
        while (j >= 0 && classSize[cand[j]] > classSize[c]) {
            cand[j + 1] = cand[j];
            j--;
        }
        cand[j + 1] = c;
    }

    uint64_t bit = 1ULL << v;
    for (int i = 0; i < nc; i++) {                       /* <= k children */
        int c = cand[i];
        int isNew = (c == used);
        uint64_t oldNbr = nbrMask[c];

        classMask[c] |= bit;
        nbrMask[c] |= G->adj[v];
        uncolored &= ~bit;
        classSize[c]++;
        color[v] = c;
        used += isNew;

        search(idx + 1, classSize[c] > curMax ? classSize[c] : curMax);

        classMask[c] &= ~bit;
        nbrMask[c] = oldNbr;
        uncolored |= bit;
        classSize[c]--;
        used -= isNew;

        if (best == target || aborted) return;           /* done: unwind */
    }
}

/* Shared setup and run. initBest: only spreads < initBest are accepted.
 * stopAt: the search ends as soon as this spread is reached. */
static void run(const Graph *g, int k, int natural, int initBest, int stopAt,
                long long nodeLimit, Result *res) {
    G = g;
    N = g->n;
    K = k;
    used = 0;
    best = initBest;
    target = stopAt;
    naturalOrder = natural;
    nodes = 0;
    budget = nodeLimit;
    aborted = 0;
    floorAvg = N / K;
    ceilAvg = (N + K - 1) / K;
    uncolored = (N == 64) ? ~0ULL : ((1ULL << N) - 1);
    memset(classMask, 0, sizeof classMask);
    memset(nbrMask, 0, sizeof nbrMask);
    memset(classSize, 0, sizeof classSize);

    /* Order by degree, highest first (insertion sort, O(n^2)).
     * Hard vertices go early, so dead branches fail near the root. */
    int deg[MAX_N];
    for (int v = 0; v < N; v++) {
        deg[v] = __builtin_popcountll(g->adj[v]);
        int j = v - 1;
        while (!natural && j >= 0 && deg[order[j]] < deg[v]) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = v;
    }

    search(0, 0);

    res->found = (best < initBest) && !aborted;
    if (res->found) memcpy(res->color, bestColor, sizeof bestColor);
}

void solve_bnb(const Graph *g, int k, Result *res) {
    run(g, k, 0, g->n + 1, spread_lower_bound(g->n, k), LLONG_MAX, res);
}

/* Replaces an optimal coloring with the lexicographically smallest optimal
 * one: vertex 1 gets the lowest color possible, then vertex 2, and so on.
 * Both solvers then print the same answer, and it matches the reference sample.
 * If the node budget runs out, the given coloring is kept, with colors
 * renumbered in order of first appearance (still optimal and valid). */
void canonicalize(const Graph *g, int k, Result *res) {
    int n = g->n, size[MAX_N] = {0}, mx = 0, mn = n;
    for (int v = 0; v < n; v++) size[res->color[v]]++;
    for (int c = 0; c < k; c++) {
        if (size[c] > mx) mx = size[c];
        if (size[c] < mn) mn = size[c];
    }
    int opt = mx - mn;

    Result canon = {0};
    run(g, k, 1, opt + 1, opt, CANON_BUDGET, &canon);
    if (canon.found) {
        *res = canon;
        return;
    }

    int relabel[MAX_N], next = 0;                       /* fallback: O(n + k) */
    for (int c = 0; c < k; c++) relabel[c] = -1;
    for (int v = 0; v < n; v++) {
        int c = res->color[v];
        if (relabel[c] < 0) relabel[c] = next++;
        res->color[v] = relabel[c];
    }
}
