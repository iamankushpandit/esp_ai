// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.
//
// Ported from therezor/cardputer-ai (MIT) -- see THIRD_PARTY.md.

// ESP32-S3 PIE (Processor Instruction Extensions) kernel for the v3 Q4_0
// row-planar dot product against Q8_0-quantized activations.
//
//   float dot_q4q8_pie(const uint8_t* nib,     // a2: 16B-aligned nibble plane
//                      const int8_t*  xq,      // a3: 16B-aligned int8 activations
//                      const uint8_t* scales,  // a4: bf16 weight scales (2B each)
//                      const float*   xs,      // a5: fp32 activation scales
//                      int            nb);     // a6: number of 32-weight blocks
//
// Per block: 16 nibble bytes hold q[0..15] (low nibbles) and q[16..31] (high
// nibbles). Both unpacked vectors get the -8 offset, then two EE.VMULAS.S8
// steps accumulate 32 signed int8 MACs into the 40-bit ACCX. The block sum
// (max |sum| = 32*8*127, fits 32 bits) is scaled by bf16(wscale)*xs[b] on the
// scalar FPU. Accumulation order matches the scalar dot_q4q8_v3 in llm.cpp:
// one float madd per block, so the two implementations agree to float
// rounding (the integer part is exact).
//
// Invariants relied on:
//   - nib and xq are 16-byte aligned (EE.VLD.128 faults otherwise; the v3
//     converter and ram_alloc16 guarantee this).
//   - Exactly one task per core runs this kernel (main + mm_worker), so the
//     per-core QACC/ACCX/QR register state is never shared. ACCX is zeroed
//     per block and no state survives across calls.
//   - SAR is set once per call (ssai 4); nothing in the loop clobbers it.

    .section .rodata
    .align  4
.Lconst:
    .byte   0x0F        // nibble mask, broadcast to all 16 lanes
    .byte   0x08        // the Q4_0 -8 offset, broadcast

    .text
    .align  4
    .global dot_q4q8_pie
    .type   dot_q4q8_pie, @function
dot_q4q8_pie:
    entry   a1, 32
    movi    a8, .Lconst
    ee.vldbc.8 q7, a8           // q7 = 0x0F splat
    addi    a8, a8, 1
    ee.vldbc.8 q6, a8           // q6 = 8 splat
    ssai    4                   // SAR = 4 for the high-nibble shift
    movi    a8, 0
    wfr     f0, a8              // acc = 0.0f
    loopnez a6, .Lloop_end
    ee.vld.128.ip q0, a2, 16    // 16 packed nibble bytes
    ee.vsr.32   q1, q0          // >> 4 (32-bit lanes; garbage masked next)
    ee.andq     q0, q0, q7      // low nibbles  -> q[0..15]
    ee.andq     q1, q1, q7      // high nibbles -> q[16..31]
    ee.vsubs.s8 q0, q0, q6      // -8
    ee.vsubs.s8 q1, q1, q6
    ee.vld.128.ip q2, a3, 16    // xq[0..15]
    ee.vld.128.ip q3, a3, 16    // xq[16..31]
    ee.zero.accx
    ee.vmulas.s8.accx q0, q2
    ee.vmulas.s8.accx q1, q3
    rur.accx_0  a7              // int32 block sum (exact)
    float.s     f2, a7, 0       // (float)isum
    l16ui       a8, a4, 0       // bf16 weight scale bits
    addi        a4, a4, 2
    slli        a8, a8, 16
    wfr         f3, a8          // wscale
    lsip        f4, a5, 4       // xs[b], post-increment
    mul.s       f3, f3, f4      // wscale * xscale
    madd.s      f0, f2, f3      // acc += isum * (wscale*xscale)
.Lloop_end:
    rfr     a2, f0
    retw.n
    .size   dot_q4q8_pie, . - dot_q4q8_pie
