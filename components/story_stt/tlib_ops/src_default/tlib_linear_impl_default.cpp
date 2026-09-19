#include "tlib_linear_impl.h"

#include <cstdint>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace tlib::ops::impl
{

    void linear_b_relu_impl_lshift(const int8_t *a, const int8_t *b, const int32_t *c, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t lshift)
    {
        int32_t res;
        const int8_t *b_curr;
        for (uint32_t row = 0; row < n; row++)
        {
            b_curr = b;
            for (uint32_t col = 0; col < m; col++)
            {
                res = std::inner_product(a, a + k, b_curr, c[col]);
                res <<= lshift;
                *y = std::clamp<int32_t>(res, 0, 127);
                y++;
                b_curr += k;
            }
            a += k;
        }
    }

    void linear_b_relu_impl_rshift(const int8_t *a, const int8_t *b, const int32_t *c, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t rshift)
    {
        int32_t res;
        const int8_t *b_curr;
        for (uint32_t row = 0; row < n; row++)
        {
            b_curr = b;
            for (uint32_t col = 0; col < m; col++)
            {
                res = std::inner_product(a, a + k, b_curr, c[col]);
                res >>= rshift;
                *y = std::clamp<int32_t>(res, 0, 127);
                y++;
                b_curr += k;
            }
            a += k;
        }
    }

    void linear_relu_impl_lshift(const int8_t *a, const int8_t *b, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t lshift)
    {
        int32_t res;
        const int8_t *b_curr;
        for (uint32_t row = 0; row < n; row++)
        {
            b_curr = b;
            for (uint32_t col = 0; col < m; col++)
            {
                res = std::inner_product(a, a + k, b_curr, 0);
                res <<= lshift;
                *y = std::clamp<int32_t>(res, 0, 127);
                y++;
                b_curr += k;
            }
            a += k;
        }
    }

    void linear_relu_impl_rshift(const int8_t *a, const int8_t *b, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t rshift)
    {
        int32_t res;
        const int8_t *b_curr;
        for (uint32_t row = 0; row < n; row++)
        {
            b_curr = b;
            for (uint32_t col = 0; col < m; col++)
            {
                res = std::inner_product(a, a + k, b_curr, 0);
                res >>= rshift;
                *y = std::clamp<int32_t>(res, 0, 127);
                y++;
                b_curr += k;
            }
            a += k;
        }
    }

    void linear_b_deq_impl(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t shift)
    {
        const float scale = 1.0f / std::powf(2.0f, shift);
        int32_t res;
        const int8_t *b_curr;
        for (uint32_t row = 0; row < n; row++)
        {
            b_curr = b;
            for (uint32_t col = 0; col < m; col++)
            {
                res = std::inner_product(a, a + k, b_curr, c[col]);
                *y = res * scale;
                y++;
                b_curr += k;
            }
            a += k;
        }
    }

    void linear_deq_impl(const int8_t *a, const int8_t *b, float *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t shift)
    {
        const float scale = 1.0f / std::powf(2.0f, shift);
        int32_t res;
        const int8_t *b_curr;
        for (uint32_t row = 0; row < n; row++)
        {
            b_curr = b;
            for (uint32_t col = 0; col < m; col++)
            {
                res = std::inner_product(a, a + k, b_curr, 0);
                *y = res * scale;
                y++;
                b_curr += k;
            }
            a += k;
        }
    }

    void linear_b_relu_deq_impl(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t shift)
    {
        const float scale = 1.0f / std::powf(2.0f, shift);
        int32_t res;
        const int8_t *b_curr;
        for (uint32_t row = 0; row < n; row++)
        {
            b_curr = b;
            for (uint32_t col = 0; col < m; col++)
            {
                res = std::inner_product(a, a + k, b_curr, c[col]);
                *y = std::max(res * scale, 0.0f);
                y++;
                b_curr += k;
            }
            a += k;
        }
    }

    void linear_impl(const float *a, const float *b, float *y, const uint32_t k, const uint32_t n, const uint32_t m)
    {
        const float *b_curr;
        for (uint32_t row = 0; row < n; row++)
        {
            b_curr = b;
            for (uint32_t col = 0; col < m; col++)
            {
                *y = std::inner_product(a, a + k, b_curr, 0.0f);
                y++;
                b_curr += k;
            }
            a += k;
        }
    }

} // tlib::ops::impl