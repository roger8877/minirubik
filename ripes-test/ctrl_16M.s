.text
main:
    li   t0, 0x20000000   # base：要寫入的起始位址
    li   t1, 0            # offset = 0
    li   t2, 16777216           # 結束值：寫 64 bytes
    li t4, 4095
loop:
    and t3, t1, t4
    add t3, t0, t3
    sw   t1, 0(t3)      # 把 offset 的值寫進去
    addi t1, t1, 4     # 前進一個 word，一個 word 是幾 bytes？
    blt  t1, t2, loop # 如果 offset < 64 就繼續
    li   a7, 10
    ecall
