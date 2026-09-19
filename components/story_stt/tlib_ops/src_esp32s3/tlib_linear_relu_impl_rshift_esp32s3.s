.text
.align  4
.global tlib_linear_relu_impl_rshift_esp32s3
.type   tlib_linear_relu_impl_rshift_esp32s3, @function

tlib_linear_relu_impl_rshift_esp32s3:
    ENTRY   a1,  16
    L8UI    a15, a1, 16
    ADDI    a12, a2, 16
    MOVI    a8,  0

    loop_row:
    BEQZ    a6, loop_row_end
    ADDI    a6, a6, -1

    MOV     a11, a7
    MOV     a13, a3
    ADDI    a2,  a12, -16

    loop_col:
    BEQZ            a11, loop_row
    ADDI            a11, a11, -1
    EE.ZERO.ACCX
    MOV             a12, a2
    EE.VLD.128.IP   q0, a12, 16

    loopnez a5, loop_col_end
    EE.VLD.128.IP               q1, a13, 16
    EE.VMULAS.S8.ACCX.LD.IP     q0, a12, 16, q0, q1

    loop_col_end:
    RUR.ACCX_0  a14
    SSR         a15
    SRA         a14, a14
    CLAMPS      a14, a14, 7
    MAX         a14, a14, a8
    S8I         a14, a4,  0
    ADDI        a4,  a4, 1
    J           loop_col

    loop_row_end:
    RETW.N

