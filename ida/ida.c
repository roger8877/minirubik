/* Host build of the IDA* solver: ./ida PPPPPPPOOOOOOO
 * Prints an optimal solution and the number of nodes the search visited.
 */
#include <stdio.h>

#include "cube.h"
#include "search.h"

int main(int argc, char **argv)
{
    state_t state;
    uint8_t path[MAX_DEPTH];
    if (argc != 2 || !parse_state(argv[1], &state)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n", argv[0]);
        return 2;
    }
    uint32_t rank = rank_state(&state);
    uint16_t p = (uint16_t) (rank / ORIENTATIONS);
    uint16_t o = (uint16_t) (rank % ORIENTATIONS);
    unsigned length = solve(p, o, path);

    /* Validate inside the program: replaying the path must reach solved. */
    for (unsigned i = 0; i < length; ++i) {
        p = perm_move[p][path[i]];
        o = orient_move[o][path[i]];
        printf("%s%s", i ? " " : "", move_names[path[i]]);
    }
    printf("\n%u moves, %lu nodes\n", length, search_nodes);
    if (p != 0 || o != 0) {
        fputs("path does not reach the solved state\n", stderr);
        return 1;
    }
    return 0;
}
