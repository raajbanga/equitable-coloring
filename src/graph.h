/* graph.h - shared types, limits, and function prototypes */
#ifndef GRAPH_H
#define GRAPH_H

#include <stddef.h>
#include <stdint.h>

#define MAX_N     64  /* one uint64_t bitmask per adjacency row */
#define DP_MAX_N  20  /* hard cap for --dp: one pass is ~3^n steps */
#define DP_AUTO_N 16  /* auto mode picks DP up to this n, B&B above it */

typedef struct {
    int n;
    uint64_t adj[MAX_N]; /* bit j of adj[i] is set <=> edge {i, j} */
} Graph;

typedef struct {
    int found;           /* 1 if a coloring using all k colors exists */
    int color[MAX_N];    /* color[v] in 0..k-1 for each vertex v */
} Result;

/* Smallest possible spread (max class - min class): 0 if k divides n, else 1. */
static inline int spread_lower_bound(int n, int k) { return (n % k == 0) ? 0 : 1; }

/* io.c */
int  read_input(const char *path, long long *k, Graph *g, char *err, size_t errlen);
int  is_connected(const Graph *g);
void print_result(const Graph *g, int k, const Result *res);

/* bnb.c */
void solve_bnb(const Graph *g, int k, Result *res);
void canonicalize(const Graph *g, int k, Result *res);

/* dp.c - returns 0 on success, -1 if memory allocation fails */
int  solve_dp(const Graph *g, int k, Result *res);

#endif
