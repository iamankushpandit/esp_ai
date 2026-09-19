#ifndef TLIB_TRANSPOSE_H_
#define TLIB_TRANSPOSE_H_

#include <cassert>

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Transposes the given matrix x by swapping the two specified dimensions.
     * Important: Currently, only transposition between adjacent dimensions is supported.
     *
     * @param x         [...] dtype tensor, input
     * @param dim0      First dimension to swap
     * @param dim1      Second dimension to swap
     * @param location  Location for output buffer
     * @returns         [...] dtype tensor, output
     */

    template <typename dtype>
    inline Tensor<dtype> transpose(const TensorView<dtype> &x, const uint8_t dim0, const uint8_t dim1, const heap::Type location = heap::kDefaultLocation)
    {
        assert(dim0 <= x.Dim());
        assert(dim1 <= x.Dim());
        assert(dim0 != dim1);

        auto y_shape = x.Shape();
        std::swap(y_shape.at(dim0), y_shape.at(dim1));
        Tensor<dtype> y(std::vector<uint32_t>(y_shape), x.Quantized(), x.Shift(), location);

        dtype *y_data{y.Data()};
        const dtype *x_data{x.DataImm()};

        const auto y_dim = y.Dim();
        auto x_stride = x.Stride();
        std::swap(x_stride.at(dim0), x_stride.at(dim1));

        // TODO: This can definitely be made more efficient
        for (uint32_t i = 0; i < x.Numel(); i++)
        {
            uint32_t flat_idx = 0;
            uint32_t remainder = i;
            for (int16_t j = y_dim - 1; j != -1; j--)
            {
                flat_idx += (remainder % y_shape.at(j)) * x_stride.at(j);
                remainder /= y_shape.at(j);
            }
            y_data[i] = x_data[flat_idx];
        }

        return y;
    }

} // tlib::ops

#endif