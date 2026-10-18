.data
input:          .string "25314672313211"
orient_weight:  .half 4860, 1620, 540, 180, 60, 20
perm_weight:    .half 7200, 1200, 240, 60, 20, 10

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
    li   a0, 10             # '\n'
    li   a7, 11             # ecall 11: print character
    ecall

    # Register use:
    # s0 = address of input
    # s1 = i, 0..5
    # s2 = j, i+1..6
    # s3 = P, permutation rank * 10
    # s4 = address of perm_weight
    # s5 = input[i], s6 = input[j], s7 = weight of digit i
    # s8 = 6, s9 = 7, loop limits

    la   s0, input
    la   s4, perm_weight
    li   s1, 0
    li   s3, 0
    li   s8, 6
    li   s9, 7

# Permutation rank (Lehmer code): for each position i, add perm_weight[i]
# once for every later digit that is smaller. The weights are the
# factorials 6!..1! pre-scaled by 10, the row index in perm_tab.
p_outer:
    add  a1, s0, s1
    lbu  s5, 0(a1)          # s5 = input[i]
    slli a2, s1, 1          # halfword offset i * 2
    add  a2, s4, a2
    lhu  s7, 0(a2)          # weight = perm_weight[i]
    addi s2, s1, 1          # j = i + 1

p_inner:
    add  a1, s0, s2
    lbu  s6, 0(a1)          # s6 = input[j]
    bgeu s6, s5, p_skip     # count only smaller digits
    add  s3, s3, s7         # P += weight
p_skip:
    addi s2, s2, 1          # j++
    blt  s2, s9, p_inner

    addi s1, s1, 1          # i++
    blt  s1, s8, p_outer

    # print P
    mv   a0, s3
    li   a7, 1
    ecall

    li   a7, 10             # exit
    ecall
