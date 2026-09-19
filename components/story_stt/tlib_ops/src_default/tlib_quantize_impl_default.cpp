#include "tlib_quantize_impl.h"

#include <cmath>
#include <cassert>
#include <cstdint>
#include <algorithm>

namespace tlib::ops::impl
{

    void quantize_impl(const float *a, int8_t *a_q, uint32_t n, uint8_t shift)
    {
        const float scale = std::powf(2.0f, shift);

        for (uint32_t i = 0; i < n; i++)
        {
            const int32_t tmp = std::round(a[i] * scale);
            a_q[i] = std::clamp<int32_t>(tmp, -128, 127);
        }
    }

} // tlib::ops::impl