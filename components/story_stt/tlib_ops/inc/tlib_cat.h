#ifndef TLIB_CAT_H_
#define TLIB_CAT_H_

#include <memory>

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Concatenates a vector of 2D tensors along the first dimension.
     *
     * @param   tensors Vector of p [n, m] dtype tensors to be concatenated
     *
     * @returns [n*p, m] dtype tensor, concatenation result
     */
    template <typename dtype>
    inline Tensor<dtype> cat(const std::vector<tlib::Tensor<dtype>> &tensors) {
        assert(tensors.at(0).Dim() == 2);
        const uint32_t dim_1 = tensors.at(0).Shape(1);

        auto get_shape_0 = [&](const uint32_t sum, const tlib::Tensor<dtype> &tensor)
        {
            assert(tensor.Dim() == 2);
            assert(tensor.Shape(1) == dim_1);
            return tensor.Shape(0) + sum;
        };

        const uint32_t dim_0 = std::accumulate(tensors.begin(), tensors.end(), 0, get_shape_0);

        Tensor<dtype> y({dim_0, dim_1}, false, 0);
        dtype *y_data{y.Data()};
        const auto max_bytes_copy{y.Bytes()};
        uint32_t n_bytes_copied{0};

        for (const auto &tensor : tensors)
        {
            const auto n_elements = tensor.Numel();
            const auto n_bytes = tensor.Bytes();
            assert(n_bytes_copied + n_bytes <= max_bytes_copy);
            std::memcpy(y_data, tensor.DataImm(), n_bytes);
            y_data += n_elements;
            n_bytes_copied += n_bytes;
        }

        return y;
    }

} // tlib::ops

#endif