/* Host build of the IDA* solver: ./ida PPPPPPPOOOOOOO
 * Prints an optimal solution (version 2 search) and how much work it took.
 */
#include <stdio.h>

#define COUNT_NODES
#include "cube.h"
#include "search2.h"

int main(int argc, char **argv)
{
    state_t state;
    uint8_t path[MAX_DEPTH2];
    if (argc != 2 || !parse_state(argv[1], &state)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n", argv[0]);
        return 2;
    }
    uint32_t rank = rank_state(&state);
    unsigned P = rank / ORIENTATIONS * 10, O = rank % ORIENTATIONS * 20;
    unsigned length = solve2(P, O, path);

    /* Validate inside the program: replaying the path must reach solved. */
    for (unsigned i = 0; i < length; ++i) {
        P = perm_row(P)[path[i]];
        O = orient_row(O)[path[i]];
        printf("%s%s", i ? " " : "", move_names[path[i]]);
    }
    printf("\n%u moves, %lu children generated, %lu frames pushed\n", length,
           v2_generated, v2_expanded);
    if (P != 0 || O != 0) {
        fputs("path does not reach the solved state\n", stderr);
        return 1;
    }
    return 0;
}
