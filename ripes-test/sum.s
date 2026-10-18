# First Ripes exercise: print the sum 1 + 2 + ... + 10.
.data
msg:    .string "sum = "

.text
main:
    li   t0, 1              # i = 1
    li   t1, 0              # sum = 0
    li   t2, 10             # upper limit
loop:
    add  t1, t1, t0         # sum += i
    addi t0, t0, 1          # i++
    ble  t0, t2, loop       # repeat while i <= 10

    la   a0, msg            # print the label
    li   a7, 4              # ecall 4: print string
    ecall

    mv   a0, t1             # print sum
    li   a7, 1              # ecall 1: print integer
    ecall

    li   a7, 10             # ecall 10: exit
    ecall
