/* IDA* search over (permutation rank, orientation rank).
 *
 * h(state) = max(perm_dist[p], orient_dist[o]). Each table is the exact
 * distance in an abstraction that ignores part of the state, so neither can
 * exceed the true distance, and neither can their maximum (checked by H1).
 *
 * Version 1: recursive and written for clarity. Stage 3 turns it into the
 * loop that the RV32I assembly follows.
 */
#ifndef SEARCH_H
#define SEARCH_H

#include <stdint.h>

#include "tables.h"

enum { MAX_DEPTH = 11, NO_FACE = 3 };

#ifdef COUNT_NODES
static unsigned long search_nodes; /* dfs() calls, for measurement only */
#endif

static inline unsigned heuristic(uint16_t p, uint16_t o)
{
    unsigned hp = perm_dist[p], ho = orient_dist[o];
    return hp > ho ? hp : ho;
}

/* Depth-first search below one node. g moves have been made so far, and
 * last_face is the face of the most recent move. Returns 1 when the solved
 * state is reached within bound moves; the path is then in path[0..g-1].
 */
static int dfs(uint16_t p, uint16_t o, unsigned g, unsigned bound,
               unsigned last_face, uint8_t path[MAX_DEPTH])
{
#ifdef COUNT_NODES
    ++search_nodes;
#endif
    unsigned h = heuristic(p, o);
    if (g + h > bound) /* cannot finish within bound: prune */
        return 0;
    if (h == 0) /* only the solved state has distance 0 in both tables */
        return 1;
    for (unsigned face = 0; face < 3; ++face) {
        if (face == last_face) /* same face twice is never shortest */
            continue;
        for (unsigned turn = 0; turn < 3; ++turn) {
            unsigned move = face * 3 + turn;
            path[g] = (uint8_t) move;
            if (dfs(perm_move[p][move], orient_move[o][move], g + 1, bound,
                    face, path))
                return 1;
        }
    }
    return 0;
}

/* Iterative deepening: try bound = h(start), h(start) + 1, ... The first bound
 * that succeeds is the optimal length, because every shorter bound was
 * searched completely and failed.
 */
static unsigned solve(uint16_t p, uint16_t o, uint8_t path[MAX_DEPTH])
{
    unsigned bound = heuristic(p, o);
    while (!dfs(p, o, 0, bound, NO_FACE, path))
        ++bound;
    return bound;
}

#endif
