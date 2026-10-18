.data
frames:         .zero 88         # 11 frames x 8 bytes
orient_weight:  .half 4860, 1620, 540, 180, 60, 20
perm_weight:    .half 7200, 1200, 240, 60, 20, 10
face_start:     .byte 0, 0, 0, 3, 3, 3, 6, 6, 6
path:           .zero 11
input:          .string "25314672313211"
names:          .string "R  "        # 4 bytes per name: name m is at names + 4m
                .string "R2 "
                .string "R' "
                .string "B  "
                .string "B2 "
                .string "B' "
                .string "D  "
                .string "D2 "
                .string "D' "
bad:            .string "\npath does not reach solved\n"

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
    li   a0, 10             # '\n'
    li   a7, 11
    ecall

    # Register use in the search:
    # s0 = frames, s1 = perm_tab, s2 = orient_tab
    # s3 = root P, s10 = root O
    # s4 = bound, s5 = d, s6 = address of the current frame
    # s7 = face_start, s8 = path, s9 = 9 (moves per node)
    # Frame layout (8 bytes): P half @0, O half @2, next @4, skip @5, rem @6

    mv   s10, t3            # keep root O in a saved register
    la   s0, frames
    la   s1, perm_tab
    la   s2, orient_tab
    la   s7, face_start
    la   s8, path
    li   s9, 9

    # h(root) = max(perm_tab row entry 9, orient_tab row entry 9)
    slli t0, s3, 1          # P * 2 = byte offset of the perm row
    add  t0, s1, t0
    lhu  t1, 18(t0)         # hp = entry 9 (9 * 2 = 18 bytes in)
    add  t2, s2, s10        # O is already a byte offset
    lhu  t4, 18(t2)         # ho = entry 9
    bgeu t1, t4, h_done
    mv   t1, t4             # h = max(hp, ho)
h_done:
    mv   s4, t1             # bound = h(root)
    beq  s4, zero, already_solved

iter:                       # one deepening iteration
    li   s5, 0              # d = 0
    mv   s6, s0             # frame 0 is at the start of frames
    sh   s3, 0(s6)          # frame.P = root P
    sh   s10, 2(s6)         # frame.O = root O
    sb   zero, 4(s6)        # frame.next = 0
    li   t0, 9
    sb   t0, 5(s6)          # frame.skip = NO_SKIP (9)
    addi t0, s4, -1
    sb   t0, 6(s6)          # frame.rem = bound - 1


# Main loop: one pass handles one move of the frame at depth d.
loop:
    slli t0, s5, 3          # d * 8
    add  s6, s0, t0         # s6 = address of frame d
    lbu  t1, 4(s6)          # m = frame.next
    lbu  t2, 5(s6)          # first move of the previous face
    bne  t1, t2, no_skip
    addi t1, t1, 3          # jump over the previous face
no_skip:
    bltu t1, s9, try_move   # m < 9: try it
    beq  s5, zero, next_bound
    addi s5, s5, -1         # all moves tried: pop to the parent frame
    j    loop

next_bound:                 # every move at the root failed
    addi s4, s4, 1          # bound++
    j    iter               # start the next iteration

try_move:
    addi t2, t1, 1
    sb   t2, 4(s6)          # frame.next = m + 1

    # Child state: entry m of this node's two rows
    slli t3, t1, 1          # m * 2 (halfword entries)
    lhu  t4, 0(s6)          # this node's P
    slli t4, t4, 1
    add  t4, s1, t4         # perm row of this node
    add  t4, t4, t3
    lhu  a3, 0(t4)          # cP = perm row entry m
    lhu  t5, 2(s6)          # this node's O
    add  t5, s2, t5         # orient row of this node (O is a byte offset)
    add  t5, t5, t3
    lhu  a4, 0(t5)          # cO = orient row entry m

    # ch = max(distance in row cP, distance in row cO)
    slli t4, a3, 1
    add  t4, s1, t4
    lhu  a5, 18(t4)         # hp
    add  t5, s2, a4
    lhu  t6, 18(t5)         # ho
    bgeu a5, t6, ch_done
    mv   a5, t6
ch_done:                    # a5 = ch
    lbu  a6, 6(s6)          # rem
    bltu a6, a5, loop       # ch > rem: prune
    add  t4, s8, s5         # address of path[d]
    sb   t1, 0(t4)          # path[d] = m
    beq  a5, zero, found    # only the solved state has h == 0

    # Push the child as frame d + 1
    addi s5, s5, 1          # d++
    addi s6, s6, 8          # next frame
    sh   a3, 0(s6)          # child.P
    sh   a4, 2(s6)          # child.O
    sb   zero, 4(s6)        # child.next = 0
    add  t4, s7, t1
    lbu  t4, 0(t4)          # face_start[m]
    sb   t4, 5(s6)          # child.skip
    addi a6, a6, -1
    sb   a6, 6(s6)          # child.rem = rem - 1
    j    loop

already_solved:
    li   s11, 0             # length = 0
    j    replay_start
found:
    addi s11, s5, 1         # length = d + 1

# Replay the path from the root: print each move and apply it with the
# same tables, so the program checks its own answer.
replay_start:
    mv   t0, s3             # P = root P
    mv   t1, s10            # O = root O
    li   t2, 0              # i = 0
    la   t3, names
replay:
    beq  t2, s11, replay_done
    add  t4, s8, t2
    lbu  t5, 0(t4)          # m = path[i]
    slli t6, t5, 2          # names are 4 bytes each
    add  a0, t3, t6         # address of the move's name
    li   a7, 4              # ecall 4: print string
    ecall

    slli t6, t5, 1          # m * 2 (halfword entries)
    slli t4, t0, 1
    add  t4, s1, t4         # perm row of P
    add  t4, t4, t6
    lhu  t0, 0(t4)          # P = perm row entry m
    add  t4, s2, t1         # orient row of O
    add  t4, t4, t6
    lhu  t1, 0(t4)          # O = orient row entry m
    addi t2, t2, 1          # i++
    j    replay

replay_done:
    li   a0, 10             # '\n'
    li   a7, 11
    ecall
    mv   a0, s11            # print the length
    li   a7, 1
    ecall
    bne  t0, zero, fail     # must end at P = 0
    bne  t1, zero, fail     # and O = 0
    li   a0, 0              # exit status 0: verified
    li   a7, 93             # ecall 93: exit with status a0
    ecall
fail:
    la   a0, bad
    li   a7, 4
    ecall
    li   a0, 1              # exit status 1: wrong answer
    li   a7, 93
    ecall
