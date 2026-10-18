# Guest-memory footprint test: store words over N = 16777216 bytes from 0x20000000.
# The address is base + (offset & mask); mask = 4095, so stores keep rewriting the same 4 KiB.
# mem_*.s and ctrl_*.s run the same instructions and differ only in the mask.
.text
main:
    li   t0, 0x20000000     # base, outside .text and .data
    li   t1, 0              # offset
    li   t2, 16777216       # N, bytes to cover
    li   t4, 4095           # mask
loop:
    and  t3, t1, t4
    add  t3, t0, t3         # address = base + (offset & mask)
    sw   t1, 0(t3)
    addi t1, t1, 4          # next word
    blt  t1, t2, loop
    li   a7, 10             # exit
    ecall
