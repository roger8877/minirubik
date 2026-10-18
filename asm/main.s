.data
input:          .string "25314672313211"
orient_weight:  .half 4860, 1620, 540, 180, 60, 20

.text
main:
    # Register use:
    # t0 = address of the first orientation digit (input + 7)
    # t1 = i, 0..5
    # t2 = loop limit (6)
    # t3 = O, orientation rank * 20
    # t4 = address of orient_weight
    # t5 = c, orientation digit 0..2
    # t6 = weight of digit i

    la   t0, input
    addi t0, t0, 7          # skip the 7 permutation digits
    li   t1, 0
    li   t2, 6
    li   t3, 0
    la   t4, orient_weight

# Orientation rank: the first 6 orientation digits as a base-3 number,
# pre-scaled by 20 so it is the byte offset of the row in orient_tab.
o_loop:
    add  a1, t0, t1
    lbu  t5, 0(a1)          # c = input[7 + i]
    addi t5, t5, -49        # '1'..'3' -> 0..2

    slli a2, t1, 1          # halfword offset i * 2
    add  a2, t4, a2
    lhu  t6, 0(a2)          # weight = orient_weight[i]

o_add:                      # add the weight c times (no multiply in RV32I)
    beq  t5, zero, o_next
    add  t3, t3, t6         # O += weight
    addi t5, t5, -1         # c--
    j    o_add

o_next:
    addi t1, t1, 1          # i++
    blt  t1, t2, o_loop     # next digit

    # print O
    mv   a0, t3
    li   a7, 1
    ecall
    li   a7, 10
    ecall
