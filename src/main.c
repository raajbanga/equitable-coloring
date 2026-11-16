/* main.c - parse arguments, run quick checks, pick a solver, print */
#include <stdio.h>
#include <string.h>

#include "graph.h"

enum { MODE_AUTO, MODE_DP, MODE_BNB };

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s <input_file> [--dp | --bnb]\n", prog);
}

int main(int argc, char **argv) {
    const char *path = NULL;
    int mode = MODE_AUTO;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dp") == 0) mode = MODE_DP;
        else if (strcmp(argv[i], "--bnb") == 0) mode = MODE_BNB;
        else if (!path) path = argv[i];
        else {
            usage(argv[0]);
            return 1;
        }
    }
    if (!path) {
        usage(argv[0]);
        return 1;
    }

    Graph g;
    long long kIn;
    char err[256];
    if (read_input(path, &kIn, &g, err, sizeof err) != 0) {
        fprintf(stderr, "error: %s\n", err);
        return 2;
    }

    Result res = {0};
    int n = g.n;

    /* k > n: every color needs at least one vertex, so no sequence. After
     * this check k <= n, so k fits in an int and log k <= log n. */
    if (kIn > n) {
        print_result(&g, 0, &res);
        return 0;
    }
    int k = (int)kIn;

    if (k == n) {
        /* Each vertex gets its own color: always proper, sequence (1,...,1). */
        for (int v = 0; v < n; v++) res.color[v] = v;
        res.found = 1;
    } else if (mode == MODE_DP || (mode == MODE_AUTO && n <= DP_AUTO_N)) {
        if (n > DP_MAX_N) {
            fprintf(stderr, "error: --dp supports n <= %d (got n = %d)\n", DP_MAX_N, n);
            return 1;
        }
        if (solve_dp(&g, k, &res) != 0) {
            fprintf(stderr, "error: out of memory for DP tables\n");
            return 3;
        }
    } else {
        solve_bnb(&g, k, &res);
    }

    /* Same optimal answer from either solver: lexicographically smallest. */
    if (res.found && k < n) canonicalize(&g, k, &res);

    print_result(&g, k, &res);
    return 0;
}
