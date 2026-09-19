#include "tlib_quantize_impl.h"

#include <cassert>

extern "C"
{

    void tlib_quantize_shift00_esp32s3(const float *a, int8_t *a_q, uint32_t k4);
    void tlib_quantize_shift01_esp32s3(const float *a, int8_t *a_q, uint32_t k4);
    void tlib_quantize_shift02_esp32s3(const float *a, int8_t *a_q, uint32_t k4);
    void tlib_quantize_shift03_esp32s3(const float *a, int8_t *a_q, uint32_t k4);
    void tlib_quantize_shift04_esp32s3(const float *a, int8_t *a_q, uint32_t k4);
    void tlib_quantize_shift05_esp32s3(const float *a, int8_t *a_q, uint32_t k4);
    void tlib_quantize_shift06_esp32s3(const float *a, int8_t *a_q, uint32_t k4);
    void tlib_quantize_shift07_esp32s3(const float *a, int8_t *a_q, uint32_t k4);
}

namespace tlib::ops::impl
{

    void quantize_impl(const float *a, int8_t *a_q, uint32_t n, uint8_t shift)
    {
        assert(n % 4 == 0);
        const uint32_t k4 = n / 4;

        switch (shift)
        {
        case 0:
            tlib_quantize_shift00_esp32s3(a, a_q, k4);
            return;
        case 1:
            tlib_quantize_shift01_esp32s3(a, a_q, k4);
            return;
        case 2:
            tlib_quantize_shift02_esp32s3(a, a_q, k4);
            return;
        case 3:
            tlib_quantize_shift03_esp32s3(a, a_q, k4);
            return;
        case 4:
            tlib_quantize_shift04_esp32s3(a, a_q, k4);
            return;
        case 5:
            tlib_quantize_shift05_esp32s3(a, a_q, k4);
            return;
        case 6:
            tlib_quantize_shift06_esp32s3(a, a_q, k4);
            return;
        case 7:
            tlib_quantize_shift07_esp32s3(a, a_q, k4);
            return;
        default:
            assert(0);
            break;
        }
    }

} // tlib::ops::impl