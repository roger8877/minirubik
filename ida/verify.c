/* Host correctness gates against the exact BFS distance of every state.
 *
 *   ./verify        H1 (admissibility) and H2 (tables complete, both layouts)
 *   ./verify --all  also H3 with the version 2 search: solve all 3,674,160
 *                   states, check every length, report search cost per
 *                   distance and the worst states, and confirm on every
 *                   distance-11 state that version 2 visits exactly the
 *                   nodes version 1 visits
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COUNT_NODES
#include "cube.h"
#include "search.h"
#include "search2.h"

enum { WORST = 10 };

static uint8_t *exact; /* exact[p * 729 + o] = true distance */

/* Full BFS over (p, o) using the same move tables as the search. */
static int build_exact(void)
{
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    exact = malloc(STATES);
    if (!queue || !exact)
        return 0;
    memset(exact, 0xFF, STATES);
    uint32_t head = 0, tail = 0;
    exact[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (unsigned m = 0; m < MOVES; ++m) {
            uint32_t there =
                (uint32_t) perm_move[p][m] * ORIENTATIONS + orient_move[o][m];
            if (exact[there] == 0xFF) {
                exact[there] = (uint8_t) (exact[here] + 1);
                queue[tail++] = there;
            }
        }
    }
    free(queue);
    return tail == STATES;
}

/* H2: every entry filled, solved entry 0, and only the solved entry 0. */
static int check_table(const char *name, const uint8_t *dist, unsigned n)
{
    unsigned max = 0, zeros = 0;
    for (unsigned i = 0; i < n; ++i) {
        if (dist[i] == 0xFF) {
            printf("H2 FAIL: %s[%u] unfilled\n", name, i);
            return 0;
        }
        if (dist[i] > max)
            max = dist[i];
        zeros += dist[i] == 0;
    }
    printf("H2 %s: %u entries, all filled, max %u, solved entry %u, "
           "%u zero entr%s\n",
           name, n, max, dist[0], zeros, zeros == 1 ? "y" : "ies");
    return dist[0] == 0 && zeros == 1;
}

/* H2: each move column of a move table must be a permutation of the ranks. */
static int check_moves(const char *name, const uint16_t *move, unsigned n)
{
    static uint8_t seen[PERMUTATIONS];
    for (unsigned m = 0; m < MOVES; ++m) {
        memset(seen, 0, sizeof seen);
        for (unsigned r = 0; r < n; ++r) {
            unsigned to = move[r * MOVES + m];
            if (to >= n || seen[to]++) {
                printf("H2 FAIL: %s column %u is not a permutation\n", name,
                       m);
                return 0;
            }
        }
    }
    printf("H2 %s: all %u columns are permutations of 0..%u\n", name, MOVES,
           n - 1);
    return 1;
}

static int check_admissible(void)
{
    unsigned long long sum_h = 0, sum_d = 0, equal = 0;
    for (uint32_t s = 0; s < STATES; ++s) {
        unsigned h = heuristic((uint16_t) (s / ORIENTATIONS),
                               (uint16_t) (s % ORIENTATIONS));
        if (h > exact[s]) {
            printf("H1 FAIL: state %u has h %u > distance %u\n", s, h,
                   exact[s]);
            return 0;
        }
        sum_h += h;
        sum_d += exact[s];
        equal += h == exact[s];
    }
    printf("H1 PASS: h <= distance for all %d states; mean h %.3f, mean "
           "distance %.3f, h exact for %llu states\n",
           STATES, (double) sum_h / STATES, (double) sum_d / STATES, equal);
    return 1;
}

/* H2 for the version 2 layout: every row must be the version 1 row scaled,
 * with the distance in entry 9.
 */
static int check_v2_tables(void)
{
    for (unsigned r = 0; r < PERMUTATIONS; ++r) {
        for (unsigned m = 0; m < MOVES; ++m)
            if (perm_tab[r][m] != perm_move[r][m] * 10U)
                return printf("H2 FAIL: perm_tab[%u][%u]\n", r, m), 0;
        if (perm_tab[r][DIST] != perm_dist[r])
            return printf("H2 FAIL: perm_tab[%u] distance\n", r), 0;
    }
    for (unsigned r = 0; r < ORIENTATIONS; ++r) {
        for (unsigned m = 0; m < MOVES; ++m)
            if (orient_tab[r][m] != orient_move[r][m] * 20U)
                return printf("H2 FAIL: orient_tab[%u][%u]\n", r, m), 0;
        if (orient_tab[r][DIST] != orient_dist[r])
            return printf("H2 FAIL: orient_tab[%u] distance\n", r), 0;
    }
    printf("H2 perm_tab/orient_tab: every row matches the version 1 tables "
           "(%zu bytes)\n",
           sizeof perm_tab + sizeof orient_tab);
    return 1;
}

static int check_all_solutions(void)
{
    unsigned long long total[12] = {0}, max_nodes[12] = {0}, count[12] = {0};
    unsigned long long pushed[12] = {0};
    unsigned long worst_nodes[WORST] = {0}, worst_pushed[WORST] = {0};
    uint32_t worst_state[WORST] = {0};
    uint8_t path[MAX_DEPTH2], path1[MAX_DEPTH];
    clock_t start = clock();
    for (uint32_t s = 0; s < STATES; ++s) {
        unsigned P = s / ORIENTATIONS * 10, O = s % ORIENTATIONS * 20;
        unsigned h0 = heuristic2(P, O);
        v2_generated = v2_expanded = 0;
        unsigned length = solve2(P, O, path);
        for (unsigned i = 0; i < length; ++i) {
            P = perm_row(P)[path[i]];
            O = orient_row(O)[path[i]];
        }
        if (length != exact[s] || P != 0 || O != 0) {
            printf("H3 FAIL: state %u: length %u, exact %u, end (%u, %u)\n",
                   s, length, exact[s], P, O);
            return 0;
        }
        /* Version 1 counts one dfs() call per node: the root once per
         * deepening iteration (bounds h0..length) plus every child generated.
         * Version 2 must visit exactly the same nodes.
         */
        unsigned long nodes = v2_generated + (length - h0 + 1);
        unsigned d = exact[s];
        if (d == 11) {
            search_nodes = 0;
            solve((uint16_t) (s / ORIENTATIONS), (uint16_t) (s % ORIENTATIONS),
                  path1);
            if (search_nodes != nodes) {
                printf("H3 FAIL: state %u: version 1 %lu nodes, version 2 "
                       "%lu\n",
                       s, search_nodes, nodes);
                return 0;
            }
        }
        ++count[d];
        total[d] += nodes;
        pushed[d] += v2_expanded;
        if (nodes > max_nodes[d])
            max_nodes[d] = nodes;
        if (d == 11 && nodes > worst_nodes[WORST - 1]) {
            unsigned i = WORST - 1;
            for (; i > 0 && worst_nodes[i - 1] < nodes; --i) {
                worst_nodes[i] = worst_nodes[i - 1];
                worst_pushed[i] = worst_pushed[i - 1];
                worst_state[i] = worst_state[i - 1];
            }
            worst_nodes[i] = nodes;
            worst_pushed[i] = v2_expanded;
            worst_state[i] = s;
        }
    }
    printf("H3 PASS: every state solved at exactly its distance by version "
           "2 (%.1f s);\n"
           "         on all distance-11 states version 1 visits the same "
           "nodes\n",
           (double) (clock() - start) / CLOCKS_PER_SEC);
    printf("nodes = root + children generated; pushed = nodes not pruned\n");
    printf("distance  states  mean nodes  max nodes  mean pushed  pushed/nodes\n");
    for (unsigned d = 0; d < 12; ++d)
        printf("%8u %7llu %11.1f %10llu %12.1f %12.3f\n", d, count[d],
               count[d] ? (double) total[d] / count[d] : 0.0, max_nodes[d],
               count[d] ? (double) pushed[d] / count[d] : 0.0,
               total[d] ? (double) pushed[d] / total[d] : 0.0);
    printf("worst distance-11 states:\n");
    for (unsigned i = 0; i < WORST; ++i) {
        state_t state;
        char text[15];
        unrank_state(worst_state[i], &state);
        format_state(&state, text);
        printf("  %s  %lu nodes, %lu pushed\n", text, worst_nodes[i],
               worst_pushed[i]);
    }
    return 1;
}

int main(int argc, char **argv)
{
    int all = argc == 2 && !strcmp(argv[1], "--all");
    if (!build_exact()) {
        fputs("exact BFS incomplete\n", stderr);
        return 1;
    }
    if (!check_table("perm_dist", perm_dist, PERMUTATIONS) ||
        !check_table("orient_dist", orient_dist, ORIENTATIONS) ||
        !check_moves("perm_move", &perm_move[0][0], PERMUTATIONS) ||
        !check_moves("orient_move", &orient_move[0][0], ORIENTATIONS) ||
        !check_v2_tables() || !check_admissible())
        return 1;
    if (all && !check_all_solutions())
        return 1;
    return 0;
}
