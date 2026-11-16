/* io.c - read and validate the input file, print the result */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"

#define TOKEN_MAX 64

/* Color c is named PALETTE[c]. Colors are numbered by first appearance
 * (color 0 holds vertex 1), so this order reproduces the reference sample:
 * {1,5} Yellow, {2,6} Blue, {3,4} Red. */
static const char *PALETTE[] = {
    "Yellow", "Blue", "Red", "Green", "Orange", "Purple",
    "Pink", "Brown", "Black", "White", "Gray", "Cyan",
};
#define PALETTE_SIZE ((int)(sizeof PALETTE / sizeof PALETTE[0]))

/* Read the next whitespace-separated token. Returns 1 on success. */
static int next_token(FILE *f, char *buf) {
    return fscanf(f, "%63s", buf) == 1;
}

/* Parse a whole token as a base-10 integer. Out-of-range values clamp to
 * LLONG_MAX / LLONG_MIN so a huge k is still "greater than n". */
static int parse_ll(const char *s, long long *out) {
    char *end;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    if (errno == ERANGE) v = (v > 0) ? LLONG_MAX : LLONG_MIN;
    *out = v;
    return 1;
}

/* Reads k, n, and the n x n matrix. Then checks: entries are 0/1, the
 * diagonal is 0, the matrix is symmetric, the graph is connected.
 * Cost: Theta(n^2) token reads + O(n^2) checks = Theta(N), linear in input. */
int read_input(const char *path, long long *k, Graph *g, char *err, size_t errlen) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(err, errlen, "cannot open file '%s'", path);
        return -1;
    }

    char tok[TOKEN_MAX];
    long long n;
    int rc = -1;

    /* skip a UTF-8 byte order mark (some editors add one) */
    unsigned char bom[3];
    if (fread(bom, 1, 3, f) != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF)
        rewind(f);

    if (!next_token(f, tok) || !parse_ll(tok, k)) {
        snprintf(err, errlen, "first value (k) is missing or not an integer");
        goto done;
    }
    if (*k <= 0) {
        snprintf(err, errlen, "k must be > 0 (got %s)", tok);
        goto done;
    }
    if (!next_token(f, tok) || !parse_ll(tok, &n)) {
        snprintf(err, errlen, "second value (n) is missing or not an integer");
        goto done;
    }
    if (n <= 0) {
        snprintf(err, errlen, "n must be > 0 (got %s)", tok);
        goto done;
    }
    if (n > MAX_N) {
        snprintf(err, errlen, "n = %lld is above the supported maximum of %d", n, MAX_N);
        goto done;
    }

    g->n = (int)n;
    memset(g->adj, 0, sizeof g->adj);

    /* n^2 entries, O(1) each */
    for (int i = 0; i < g->n; i++) {
        for (int j = 0; j < g->n; j++) {
            if (!next_token(f, tok)) {
                snprintf(err, errlen,
                         "matrix has fewer than %lld entries (missing row %d, column %d)",
                         n * n, i + 1, j + 1);
                goto done;
            }
            if (strcmp(tok, "1") == 0) {
                g->adj[i] |= 1ULL << j;
            } else if (strcmp(tok, "0") != 0) {
                snprintf(err, errlen,
                         "matrix entry at row %d, column %d is '%s', expected 0 or 1",
                         i + 1, j + 1, tok);
                goto done;
            }
        }
    }
    if (next_token(f, tok)) {
        snprintf(err, errlen, "extra data after the %lldx%lld matrix: '%s'", n, n, tok);
        goto done;
    }

    /* n diagonal checks + n(n-1)/2 symmetry checks.
     * A one-sided entry is treated as an edge in both directions, with a
     * warning on stderr. */
    for (int i = 0; i < g->n; i++) {
        if ((g->adj[i] >> i) & 1ULL) {
            snprintf(err, errlen, "self-loop: diagonal entry at row %d is 1", i + 1);
            goto done;
        }
        for (int j = i + 1; j < g->n; j++) {
            if (((g->adj[i] >> j) & 1ULL) != ((g->adj[j] >> i) & 1ULL)) {
                fprintf(stderr,
                        "warning: matrix is not symmetric at row %d, column %d; "
                        "treating it as edge %d-%d\n", i + 1, j + 1, i + 1, j + 1);
                g->adj[i] |= 1ULL << j;
                g->adj[j] |= 1ULL << i;
            }
        }
    }

    /* Input graphs are expected to be connected, but both solvers work without it,
     * so a disconnected graph gets a warning, not an error. */
    if (!is_connected(g))
        fprintf(stderr, "warning: graph is not connected; solving it anyway\n");
    rc = 0;

done:
    fclose(f);
    return rc;
}

/* BFS using bitmasks. Each vertex enters the frontier once: O(n) mask ops. */
int is_connected(const Graph *g) {
    uint64_t all = (g->n == 64) ? ~0ULL : ((1ULL << g->n) - 1);
    uint64_t seen = 1ULL, frontier = 1ULL; /* start at vertex 0 */

    while (frontier) {
        uint64_t next = 0;
        while (frontier) {
            int v = __builtin_ctzll(frontier);
            frontier &= frontier - 1;
            next |= g->adj[v];
        }
        frontier = next & ~seen;
        seen |= frontier;
    }
    return seen == all;
}

/* 1 if color a's line goes before color b's when their sizes tie:
 * palette names alphabetically, then "Color j" names by number. */
static int name_before(int a, int b) {
    if (a < PALETTE_SIZE && b < PALETTE_SIZE) return strcmp(PALETTE[a], PALETTE[b]) < 0;
    if (a < PALETTE_SIZE || b < PALETTE_SIZE) return a < PALETTE_SIZE;
    return a < b;
}

/* Prints the sequence, then one line per color class.
 * Lines are ordered by size (largest first), ties by color name, so they
 * line up with the sequence and match the reference sample.
 * Vertices are printed 1-indexed in ascending order. Cost: O(n + k^2), k <= n. */
void print_result(const Graph *g, int k, const Result *res) {
    if (!res->found) {
        puts("no such a sequence exists");
        return;
    }

    int n = g->n;
    int size[MAX_N] = {0}, order[MAX_N];
    int members[MAX_N][MAX_N]; /* members[c] = vertices with color c, ascending */

    for (int c = 0; c < k; c++) order[c] = c;
    for (int v = 0; v < n; v++) {
        int c = res->color[v];
        members[c][size[c]++] = v;
    }

    /* insertion sort: size descending, then by name */
    for (int i = 1; i < k; i++) {
        int c = order[i], j = i - 1;
        while (j >= 0 && (size[order[j]] < size[c] ||
                          (size[order[j]] == size[c] && name_before(c, order[j])))) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = c;
    }

    printf("(");
    for (int i = 0; i < k; i++) printf(i ? ",%d" : "%d", size[order[i]]);
    printf(")\n");

    char name[MAX_N][16];
    int width = 0;
    for (int c = 0; c < k; c++) {
        if (c < PALETTE_SIZE) snprintf(name[c], sizeof name[c], "%s", PALETTE[c]);
        else snprintf(name[c], sizeof name[c], "Color %d", c + 1);
        int len = (int)strlen(name[c]);
        if (len > width) width = len;
    }

    for (int i = 0; i < k; i++) {
        int c = order[i];
        printf("%-*s : ", width, name[c]);
        for (int t = 0; t < size[c]; t++) printf(t ? ", %d" : "%d", members[c][t] + 1);
        printf("\n");
    }
}
