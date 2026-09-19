#include "tlib_conv1d.h"

#include <cassert>

#include <tlib_heap.h>
#include <tlib_linear.h>
#include <tlib_linear_impl.h>

namespace tlib::ops
{

    /**
     * Extracts sliding windows for a single channel into a 2D tensor.
     *
     * ic           input channels
     * in_len       input length
     * out_len      output length
     *
     * @param y             [out_len, k] int8_t tensor, output
     * @param pad_buffer    [in_lin+k+1] int8_t tensor, buffer for padding
     * @param x             [ic, in_len] int8_t tensor, input
     * @param ci            Channel index to unfold
     * @param k             Kernel size
     */
    void unfold_channel(Tensor<int8_t> &y, Tensor<int8_t> &pad_buffer, const TensorView<int8_t> &x, const uint32_t ci, const uint8_t k)
    {

        assert(y.Shape(1) == k);

        const uint32_t in_len = x.Shape(1);
        const uint8_t p = (k - 1) / 2;
        assert(p != 0);

        const int8_t *x_data{x.DataImm()};
        int8_t *y_data{y.Data()}, *pad_data{pad_buffer.Data()};
        const uint32_t ch_offset{ci * in_len};

        const size_t padded_len{pad_buffer.Numel()};

        // pre-pad channel data with p zeros on the left and k-1-p zeros on the right
        std::memset(pad_data, 0, p);
        std::memcpy(pad_data + p, x_data + ch_offset, in_len);
        std::memset(pad_data + p + in_len, 0, padded_len - p - in_len);

        for (uint32_t i = 0; i < in_len; i++)
        {
            std::memcpy(y_data, pad_data, k);
            y_data += k;
            pad_data += 1;
        }
    }

    Tensor<float> conv1d_dw_deq(const TensorView<int8_t> &x, const TensorView<int8_t> &w, const TensorView<int32_t> &b)
    {
        assert(x.Dim() == 2);
        assert(w.Dim() == 3);
        assert(b.Dim() == 1);
        assert(w.Shape(1) == 1);
        assert(x.Shape(0) == w.Shape(0));
        assert(x.Shape(0) == b.Shape(0));

        // Output channels
        const uint32_t oc{w.Shape(0)};
        // Kernel size
        const uint32_t k{w.Shape(2)};
        // Input length (=output length)
        const uint32_t in_len{x.Shape(1)};

        Tensor<float> y{{oc, in_len}, false, 0};
        // Buffer for storing unfolded channel data
        Tensor<int8_t> x_ch{{in_len, k}, true, x.Shift(), tlib::heap::Type::SRAM};
        // Buffer for storing intermediate padding data
        Tensor<int8_t> pad_buffer{{in_len + k - 1, 1}, false, 0, tlib::heap::Type::SRAM};

        const uint8_t shift = x.Shift() + w.Shift();

        // TODO: Can also fuse batchnorm into this by altering weight and bias during export.

        for (uint32_t i = 0; i < oc; i++)
        {
            unfold_channel(x_ch, pad_buffer, x, i, k);

            const int8_t *a_data = x_ch.DataImm();
            const int8_t *b_data = w.DataImm() + i * k;
            const int32_t *c_data = b.DataImm() + i;
            float *y_data = y.Data() + i * in_len;
            impl::linear_b_deq_impl(a_data, b_data, c_data, y_data, k, in_len, 1, shift);
        }

        return y;
    }

} // tlib::ops