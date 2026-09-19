.text
.align  4
.global tlib_quantize_shift03_esp32s3
.type   tlib_quantize_shift03_esp32s3, @function

tlib_quantize_shift03_esp32s3:
    ENTRY   a1, 16
    
    loopnez a4, loop_end
    EE.LDF.128.IP   f3, f2, f1, f0, a2, 16
    ROUND.S         a5, f0, 3
    CLAMPS          a5, a5, 7
    S8I             a5, a3, 0
    ADDI.N          a3, a3, 1

    ROUND.S         a6, f1, 3
    CLAMPS          a6, a6, 7
    S8I             a6, a3, 0
    ADDI.N          a3, a3, 1

    ROUND.S         a7, f2, 3
    CLAMPS          a7, a7, 7
    S8I             a7, a3, 0
    ADDI.N          a3, a3, 1

    ROUND.S         a8, f3, 3
    CLAMPS          a8, a8, 7
    S8I             a8, a3, 0
    ADDI.N          a3, a3, 1

    loop_end:
    RETW.N
