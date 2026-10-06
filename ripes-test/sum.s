.data                     # 以下是資料區
msg:    .string "sum = "  # 一個字串，標籤 msg 代表它的位址

.text                     # 以下是程式碼區
main:
    li   t0, 1            # i = 1        (li = load immediate，把常數放進暫存器)
    li   t1, 0            # sum = 0
    li   t2, 10           # 上限 10
loop:                     # 標籤：迴圈開頭的位置
    add  t1, t1, t0       # sum = sum + i
    addi t0, t0, 1        # i = i + 1
    ble  t0, t2, loop     # 如果 i <= 10，跳回 loop

    la   a0, msg          # a0 = 字串的位址 (la = load address)
    li   a7, 4            # 功能 4：印字串
    ecall

    mv   a0, t1           # a0 = sum     (mv = 複製暫存器)
    li   a7, 1            # 功能 1：印整數
    ecall

    li   a7, 10           # 功能 10：結束
    ecall
