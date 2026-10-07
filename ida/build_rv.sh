#!/bin/sh
# Build the gcc reference ELFs for Ripes: rv1_<state>.elf (version 1) and
# rv2_<state>.elf (version 2) for every state given on the command line.
#   sh build_rv.sh 21345671111111 54721631111111
set -e
CC=riscv64-unknown-elf-gcc
FLAGS="-O2 -march=rv32i -mabi=ilp32 -ffreestanding -nostdlib -nostartfiles \
-Wl,--no-relax -Wl,-e,_start -Wall -Wextra"
for state in "$@"; do
    $CC $FLAGS -DINPUT="\"$state\"" -o "rv2_$state.elf" ida_rv.c
    $CC $FLAGS -DV1 -DINPUT="\"$state\"" -o "rv1_$state.elf" ida_rv.c
done
riscv64-unknown-elf-size rv*.elf
echo "mul/div/rem instructions: $(riscv64-unknown-elf-objdump -d rv*.elf |
    grep -cE '[[:space:]](mul|div|rem)[a-z]*[[:space:]]' || true)"
