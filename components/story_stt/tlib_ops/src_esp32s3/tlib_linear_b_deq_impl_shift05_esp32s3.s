.text
.align  4
.global tlib_linear_b_deq_impl_shift05_esp32s3
.type   tlib_linear_b_deq_impl_shift05_esp32s3, @function

tlib_linear_b_deq_impl_shift05_esp32s3:
    ENTRY   a1,  16
    ADDI    a12, a2, 16
    L32I.N  a9,  a1, 16
    L8UI    a10, a1, 20

    loop_row:
    BEQZ    a7, loop_row_end
    ADDI    a7, a7, -1

    MOV     a11, a9
    MOV     a13, a3
    ADDI    a2,  a12, -16
    MOV     a15, a4

    loop_col:
    BEQZ            a11, loop_row
    ADDI            a11, a11, -1
    EE.ZERO.ACCX
    MOV             a12, a2
    L32I.N          a8 , a15, 0
    ADDI            a15, a15, 4
    EE.VLD.128.IP   q0, a12, 16

    loopnez a6, loop_col_end
    EE.VLD.128.IP               q1, a13, 16
    EE.VMULAS.S8.ACCX.LD.IP     q0, a12, 16, q0, q1

    loop_col_end:
    RUR.ACCX_0  a14
    ADD.N       a14, a14, a8
    FLOAT.S     f1,  a14, 5
    SSI         f1,  a5, 0
    ADDI        a5,  a5, 4
    J           loop_col

    loop_row_end:
    RETW.N
