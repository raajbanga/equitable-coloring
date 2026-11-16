/* dp.c - DP over subsets (n <= DP_MAX_N)
 *
 * Question asked per pass: can V be split into exactly k independent sets,
 * each with size in [lo, hi]?
 *   f[S] = bitset over j; bit j is set <=> S splits into j such sets.
 *   f[{}] = {0}
 *   f[S]  = OR over T subset of S with lowest(S) in T, T independent,
 *           lo <= |T| <= hi, of (f[S \ T] shifted left by 1)
 * Fixing lowest(S) in T means each partition is built in only one order.
 * One pass = O(3^n) word ops (each vertex is in S \ T, in T, or not in S).
 *
 * Driver: one pass with [1, n] tests plain k-colorability. Then for
 * d = lower bound, lower bound + 1, ...: try every lo with
 * lo <= floor(n/k) <= ceil(n/k) <= lo + d. The first success is optimal. */
#include <stdlib.h>

#include "graph.h"

/* Fills f for the given [lo, hi]. Returns 1 if V splits into k sets. */
static int fill(const uint8_t *indep, uint32_t *f, int n, int k, int lo, int hi) {
    uint32_t full = (1u << n) - 1;
    uint32_t jmask = (1u << (k + 1)) - 1; /* keep bits 0..k; k < n <= 20 */

    f[0] = 1u;
    for (uint32_t S = 1; S <= full; S++) {               /* 2^n subsets */
        uint32_t low = S & (~S + 1), rest = S ^ low, acc = 0, sub = rest;
        for (;;) {                                       /* submasks of rest */
            uint32_t T = sub | low;
            int t = __builtin_popcount(T);
            if (t >= lo && t <= hi && indep[T]) acc |= f[S ^ T] << 1;
            if (sub == 0) break;
            sub = (sub - 1) & rest;
        }
        f[S] = acc & jmask;
    }
    return (int)((f[full] >> k) & 1u);
}

/* Walks back from V: picks any T that leads to a valid split of the rest.
 * k steps, each scans at most 2^(n-1) submasks: O(k * 2^n). */
static void rebuild(const uint8_t *indep, const uint32_t *f, int n, int k,
                    int lo, int hi, Result *res) {
    uint32_t S = (1u << n) - 1;
    int j = k, c = 0;

    while (S) {
        uint32_t low = S & (~S + 1), rest = S ^ low, sub = rest;
        for (;;) {
            uint32_t T = sub | low;
            int t = __builtin_popcount(T);
            if (t >= lo && t <= hi && indep[T] && ((f[S ^ T] >> (j - 1)) & 1u)) {
                for (uint32_t m = T; m; m &= m - 1) res->color[__builtin_ctz(m)] = c;
                S ^= T;
                j--;
                c++;
                break;
            }
            if (sub == 0) break; /* unreachable: f says some T works */
            sub = (sub - 1) & rest;
        }
    }
    res->found = 1;
}

int solve_dp(const Graph *g, int k, Result *res) {
    int n = g->n;
    size_t count = (size_t)1 << n;
    uint8_t *indep = malloc(count);
    uint32_t *f = malloc(count * sizeof *f);
    if (!indep || !f) {
        free(indep);
        free(f);
        return -1;
    }

    /* indep[S] = 1 if no edge inside S. Built from S minus its lowest vertex v:
     * O(1) per subset, O(2^n) total. */
    indep[0] = 1;
    for (uint32_t S = 1; S < count; S++) {
        int v = __builtin_ctz(S);
        indep[S] = indep[S & (S - 1)] && !(g->adj[v] & S);
    }

    res->found = 0;

    /* Pass 0: sizes in [1, n] puts no size limit on classes, so this tests
     * whether any coloring with exactly k colors exists. */
    if (!fill(indep, f, n, k, 1, n)) goto done;

    int floorAvg = n / k, ceilAvg = (n + k - 1) / k;
    for (int d = spread_lower_bound(n, k); !res->found; d++) {
        /* lo <= floor(n/k) keeps lo*k <= n; lo + d >= ceil(n/k) keeps hi*k >= n.
         * d = n - 1, lo = 1 is pass 0 again, so the loop always ends. */
        for (int lo = floorAvg; lo >= 1 && lo + d >= ceilAvg; lo--) {
            if (fill(indep, f, n, k, lo, lo + d)) {
                rebuild(indep, f, n, k, lo, lo + d, res);
                break;
            }
        }
    }

done:
    free(indep);
    free(f);
    return 0;
}
