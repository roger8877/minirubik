/* IDA* search, version 2: the loop that the RV32I assembly follows.
 *
 * Same search as search.h (same heuristic, same move order, same pruning),
 * restructured for a target without multiply or recursion:
 *
 *   A. an explicit stack of MAX_DEPTH frames replaces recursion;
 *   B. a child's h is checked in the parent's loop, so a pruned child is
 *      never pushed;
 *   C. a state is the pair (P, O) of pre-scaled row positions in perm_tab and
 *      orient_tab, whose entry 9 is the distance: one table row per half of
 *      the state, no separate distance tables and no index multiply;
 *   D. each frame stores the moves still allowed below it, so a child is
 *      pruned with a single compare h > rem;
 *   E. moves run 0..8 in one loop; the previous face is skipped by jumping
 *      over its three moves.
 *
 * Define COUNT_NODES to count children generated and frames pushed.
 */
#ifndef SEARCH2_H
#define SEARCH2_H

#include <stdint.h>

#include "tables2.h"

enum { MAX_DEPTH2 = 11, N_MOVES = 9, DIST = 9, NO_SKIP = 9 };

#ifdef COUNT_NODES
static unsigned long v2_generated, v2_expanded;
#define COUNT(x) (++(x))
#else
#define COUNT(x) ((void) 0)
#endif

/* First move of the face of each move: the move to jump over next. */
static const uint8_t face_start[9] = {0, 0, 0, 3, 3, 3, 6, 6, 6};

/* P = permutation rank * 10: index of the row in halfwords.
 * O = orientation rank * 20: offset of the row in bytes.
 */
static inline const uint16_t *perm_row(unsigned P)
{
    return &perm_tab[0][0] + P;
}

static inline const uint16_t *orient_row(unsigned O)
{
    return (const uint16_t *) ((const uint8_t *) &orient_tab[0][0] + O);
}

static inline unsigned heuristic2(unsigned P, unsigned O)
{
    unsigned hp = perm_row(P)[DIST], ho = orient_row(O)[DIST];
    return hp > ho ? hp : ho;
}

typedef struct {
    uint16_t P, O;   /* this node */
    uint8_t next;    /* next move to try */
    uint8_t skip;    /* first move of the previous face, or NO_SKIP */
    uint8_t rem;     /* moves still allowed after this node's children */
} frame_t;

/* Returns the optimal length; path[0..length-1] holds the moves. */
static unsigned solve2(unsigned P, unsigned O, uint8_t path[MAX_DEPTH2])
{
    frame_t stack[MAX_DEPTH2];
    unsigned h = heuristic2(P, O);
    if (h == 0)
        return 0;
    for (unsigned bound = h;; ++bound) {
        unsigned d = 0;
        stack[0].P = (uint16_t) P;
        stack[0].O = (uint16_t) O;
        stack[0].next = 0;
        stack[0].skip = NO_SKIP;
        stack[0].rem = (uint8_t) (bound - 1);
        COUNT(v2_expanded);
        for (;;) {
            frame_t *f = &stack[d];
            unsigned m = f->next;
            if (m == f->skip)
                m += 3;
            if (m >= N_MOVES) { /* all children tried: pop */
                if (d == 0)
                    break; /* bound exhausted */
                --d;
                continue;
            }
            f->next = (uint8_t) (m + 1);
            unsigned cP = perm_row(f->P)[m];
            unsigned cO = orient_row(f->O)[m];
            unsigned ch = heuristic2(cP, cO);
            COUNT(v2_generated);
            if (ch > f->rem) /* cannot finish within bound: prune */
                continue;
            path[d] = (uint8_t) m;
            if (ch == 0) /* only the solved state has h == 0 */
                return d + 1;
            frame_t *c = &stack[++d]; /* push the child */
            c->P = (uint16_t) cP;
            c->O = (uint16_t) cO;
            c->next = 0;
            c->skip = face_start[m];
            c->rem = (uint8_t) (f->rem - 1);
            COUNT(v2_expanded);
        }
    }
}

#endif
