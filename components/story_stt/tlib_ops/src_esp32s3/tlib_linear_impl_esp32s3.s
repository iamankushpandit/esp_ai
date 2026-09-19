.text
.align  4
.global tlib_linear_impl_esp32s3
.type   tlib_linear_impl_esp32s3, @function

tlib_linear_impl_esp32s3:
    ENTRY   a1,  16
    ADDI    a12, a2, 16

    loop_row:
    BEQZ    a6, loop_row_end
    ADDI    a6, a6, -1

    MOV     a11, a7
    MOV     a13, a3
    ADDI    a2,  a12, -16

    loop_col:
    BEQZ            a11, loop_row
    ADDI            a11, a11, -1
    CONST.S         f8,  0
    CONST.S         f9,  0
    CONST.S         f10, 0
    CONST.S         f11, 0
    MOV             a12, a2
    EE.LDF.128.IP   f7,  f6, f5, f4, a12, 16

    loopnez a5, loop_col_end
    EE.LDF.128.IP   f3,  f2, f1, f0, a13, 16
    MADD.S          f8,  f7, f3
    MADD.S          f9,  f6, f2
    MADD.S          f10, f5, f1
    MADD.S          f11, f4, f0
    EE.LDF.128.IP   f7,  f6, f5, f4, a12, 16

    loop_col_end:
    add.s       f8, f8, f9
    add.s       f8, f8, f10
    add.s       f8, f8, f11
    SSI         f8, a4, 0
    ADDI        a4, a4, 4
    J           loop_col

    loop_row_end:
    RETW.N
