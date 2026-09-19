#include "tlib_conv2d.h"

#include <tlib_heap.h>
#include <tlib_linear.h>
#include <tlib_transpose.h>

namespace tlib::ops
{

    Tensor<int8_t> unfold(const TensorView<int8_t> &x, const uint8_t kh, const uint8_t kw,
                          const uint8_t sh, const uint8_t sw, const uint8_t ph, const uint8_t pw,
                          const uint8_t dh, const uint8_t dw, const uint8_t oh, const uint8_t ow, const heap::Type location)
    {

        assert(x.Dim() == 3);

        const int32_t h{static_cast<int32_t>(x.Shape(1))}, w{static_cast<int32_t>(x.Shape(2))};
        const uint32_t out_rows = oh * ow, out_cols{x.Shape(0) * kh * kw};

        Tensor<int8_t> y({out_rows, out_cols}, x.Quantized(), x.Shift(), location);

        const int8_t *x_data{x.DataImm()};
        int8_t *y_data{y.Data()};
        uint32_t oi, oj, h_base, w_base, ci, ki, kj;
        int32_t h_src, w_src;

        for (uint32_t r = 0; r < out_rows; r++)
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

        // Unfold input tensor
        auto x_unfolded = unfold(x, kh, kw, sh, sw, ph, pw, dh, dw, oh, ow, heap::Type::SRAM);

        // Calculate convolution result
        auto y = linear_relu(w, x_unfolded, y_shift);

        // View result with correct shape
        y.View({oc, oh, ow});

        return y;
    }

} // tlib::ops