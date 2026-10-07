/* Freestanding RV32I build of the solver for Ripes, compiled by gcc as the
 * reference the hand-written assembly is compared against.
 *
 *   riscv64-unknown-elf-gcc -O2 -march=rv32i -mabi=ilp32 ... -DINPUT=...
 *   -DV1 selects the recursive version 1 search instead of version 2.
 *
 * No libc and no libgcc: input parsing and ranking use only adds and shifts,
 * and console output goes through Ripes environment calls.
 */
#include <stdint.h>

#ifdef V1
#include "search.h"
#define SOLVE(P, O, path) solve((uint16_t) (P), (uint16_t) (O), path)
#define NEXT_P(P, m) perm_move[P][m]
#define NEXT_O(O, m) orient_move[O][m]
/* Weights of each Lehmer digit and orientation digit, unscaled ranks. */
static const uint16_t perm_weight[6] = {720, 120, 24, 6, 2, 1};
static const uint16_t orient_weight[6] = {243, 81, 27, 9, 3, 1};
#else
#include "search2.h"
#define SOLVE(P, O, path) solve2(P, O, path)
#define NEXT_P(P, m) perm_row(P)[m]
#define NEXT_O(O, m) orient_row(O)[m]
/* The same weights pre-scaled: P = rank * 10, O = rank * 20. */
static const uint16_t perm_weight[6] = {7200, 1200, 240, 60, 20, 10};
static const uint16_t orient_weight[6] = {4860, 1620, 540, 180, 60, 20};
#endif

#ifndef INPUT
#define INPUT "21345671111111"
#endif

static const char input[] = INPUT;
static const char *const names[9] = {"R ",  "R2 ", "R' ", "B ", "B2 ",
                                     "B' ", "D ",  "D2 ", "D' "};

static void print_str(const char *s)
{
    register const char *a0 __asm__("a0") = s;
    register int a7 __asm__("a7") = 4;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

static void print_int(int v)
{
    register int a0 __asm__("a0") = v;
    register int a7 __asm__("a7") = 1;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

static __attribute__((noreturn)) void exit_with(int code)
{
    register int a0 __asm__("a0") = code;
    register int a7 __asm__("a7") = 93;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    for (;;)
        ;
}

/* Validates the 14-digit input and ranks it. Returns 0 if invalid. */
static int parse(unsigned *P, unsigned *O)
{
    uint8_t p[7];
    unsigned seen = 0, sum = 0, rp = 0, ro = 0;
    for (unsigned i = 0; i < 7; ++i) {
        unsigned c = (unsigned) (input[i] - '1');
        if (c >= 7 || (seen >> c & 1))
            return 0;
        seen |= 1U << c;
        p[i] = (uint8_t) c;
    }
    for (unsigned i = 0; i < 7; ++i) {
        unsigned c = (unsigned) (input[7 + i] - '1');
        if (c >= 3)
            return 0;
        sum += c;
        if (i < 6) /* orientation rank: add the digit's weight c times */
            for (; c; --c)
                ro += orient_weight[i];
    }
    if (input[14] != '\0')
        return 0;
    while (sum >= 3) /* sum <= 14, so this is mod 3 without a divide */
        sum -= 3;
    if (sum)
        return 0;
    for (unsigned i = 0; i < 6; ++i) /* Lehmer code, weights added */
        for (unsigned j = i + 1; j < 7; ++j)
            if (p[j] < p[i])
                rp += perm_weight[i];
    *P = rp;
    *O = ro;
    return 1;
}

static int main_rv(void)
{
    unsigned P, O;
    uint8_t path[11];
    if (!parse(&P, &O)) {
        print_str("invalid state\n");
        return 2;
    }
    unsigned length = SOLVE(P, O, path);
    for (unsigned i = 0; i < length; ++i) { /* replay to validate */
        print_str(names[path[i]]);
        P = NEXT_P(P, path[i]);
        O = NEXT_O(O, path[i]);
    }
    print_str("\nmoves: ");
    print_int((int) length);
    print_str("\n");
    if (P != 0 || O != 0) {
        print_str("path does not reach solved\n");
        return 1;
    }
    return 0;
}

void _start(void)
{
    exit_with(main_rv());
}
