#include "tlib_conv2d.h"

#include <tlib_heap.h>
#include <tlib_linear.h>
#include <tlib_transpose.h>

#include <cstring>

namespace tlib::ops
{

    Tensor<int8_t> unfold(const TensorView<int8_t> &x, const uint8_t kh, const uint8_t kw,
                          const uint8_t sh, const uint8_t sw, const uint8_t ph, const uint8_t pw,
                          const uint8_t dh, const uint8_t dw, const uint8_t oh, const uint8_t ow, const heap::Type location,
                          uint32_t r0 = 0, uint32_t n_rows = 0)
    {

        assert(x.Dim() == 3);

        const int32_t h{static_cast<int32_t>(x.Shape(1))}, w{static_cast<int32_t>(x.Shape(2))};
        // story: optional row window [r0, r0 + n_rows) so callers can unfold in tiles.
        const uint32_t out_cols{x.Shape(0) * kh * kw};
        if (n_rows == 0) n_rows = oh * ow - r0;
        const uint32_t out_rows = r0 + n_rows;

        Tensor<int8_t> y({n_rows, out_cols}, x.Quantized(), x.Shift(), location);

        const int8_t *x_data{x.DataImm()};
        int8_t *y_data{y.Data()};
        uint32_t oi, oj, h_base, w_base, ci, ki, kj;
        int32_t h_src, w_src;

        for (uint32_t r = r0; r < out_rows; r++)
        {
            oi = r / ow;
            oj = r % ow;
            h_base = oi * sh - ph;
            w_base = oj * sw - pw;

            for (uint32_t c = 0; c < out_cols; c++)
            {
                ci = c / (kh * kw);
                ki = (c % (kh * kw)) / kw;
                kj = c % kw;
                h_src = h_base + dh * ki;
                w_src = w_base + dw * kj;
                if (h_src < 0 || h_src >= h || w_src < 0 || w_src >= w)
                {
                    *y_data = 0;
                }
                else
                {
                    *y_data = *(x_data + ci * h * w + h_src * w + w_src);
                }
                y_data++;
            }
        }

        return y;
    }

    Tensor<int8_t> conv2d_relu(const TensorView<int8_t> &x, const TensorView<int8_t> &w, const TensorView<int32_t> &b,
                               const uint8_t oc, const uint8_t kh, const uint8_t kw, const uint8_t sh, const uint8_t sw,
                               const uint8_t ph, const uint8_t pw, const uint8_t dh, const uint8_t dw, const uint8_t y_shift)
    {

        // Get input and output height and width
        const uint32_t ih = x.Shape(1);
        const uint32_t iw = x.Shape(2);
        const uint32_t oh = (ih + 2 * ph - dh * (kh - 1) - 1) / sh + 1;
        const uint32_t ow = (iw + 2 * pw - dw * (kw - 1) - 1) / sw + 1;

        // Unfold input tensor
        auto x_unfolded = unfold(x, kh, kw, sh, sw, ph, pw, dh, dw, oh, ow, heap::Type::PSRAM);

        // Calculate transposed convolution result
        auto y_T = linear_relu(x_unfolded, w, b, y_shift);

        // Transpose result
        auto y = transpose(y_T, 0, 1);

        // View result with correct shape
        y.View({oc, oh, ow});

        return y;
    }

    Tensor<int8_t> conv2d_relu(const TensorView<int8_t> &x, const TensorView<int8_t> &w,
                               const uint8_t oc, const uint8_t kh, const uint8_t kw, const uint8_t sh, const uint8_t sw,
                               const uint8_t ph, const uint8_t pw, const uint8_t dh, const uint8_t dw, const uint8_t y_shift)
    {

        // Get input and output height and width
        const uint32_t ih = x.Shape(1);
        const uint32_t iw = x.Shape(2);
        const uint32_t oh = (ih + 2 * ph - dh * (kh - 1) - 1) / sh + 1;
        const uint32_t ow = (iw + 2 * pw - dw * (kw - 1) - 1) / sw + 1;

        // story: the unfolded input is the streamed operand of the matmul and
        // must sit in SRAM to be fast. If the whole matrix doesn't fit the SRAM
        // heap, unfold and multiply in row tiles (multiples of 16) that do.
        const uint32_t rows = oh * ow, cols = x.Shape(0) * kh * kw;
        const size_t budget = heap::LargestFree(heap::Type::SRAM);
        uint32_t tile = rows;
        if ((size_t)rows * cols > budget)
        {
            tile = (uint32_t)(budget / cols) & ~15u;
            if (tile == 0) tile = 16; // falls back to PSRAM, but still correct
        }
        if (tile >= rows)
        {
            auto x_unfolded = unfold(x, kh, kw, sh, sw, ph, pw, dh, dw, oh, ow, heap::Type::SRAM);
            auto y = linear_relu(w, x_unfolded, y_shift);
            y.View({oc, oh, ow});
            return y;
        }

        Tensor<int8_t> y({oc, rows}, true, y_shift);
        int8_t *y_data = y.Data();
        for (uint32_t r0 = 0; r0 < rows; r0 += tile)
        {
            const uint32_t n = rows - r0 < tile ? rows - r0 : tile;
            Tensor<int8_t> part;
            {
                auto x_tile = unfold(x, kh, kw, sh, sw, ph, pw, dh, dw, oh, ow, heap::Type::SRAM, r0, n);
                part = linear_relu(w, x_tile, y_shift); // [oc, n]
            }
            const int8_t *p = part.DataImm();
            for (uint32_t o = 0; o < oc; o++)
                memcpy(y_data + (size_t)o * rows + r0, p + (size_t)o * n, n);
        }
        y.View({oc, oh, ow});
        return y;
    }

} // tlib::ops