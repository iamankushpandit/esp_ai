#ifndef TLIB_ADD_H_
#define TLIB_ADD_H_

#include <cstdint>
#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * @brief Adds two tensors of the same shape.
     *
     * @param a     [...] float tensor, input a
     * @param b     [...] float tensor, input b
     *
     * @returns     a + b
     */
    Tensor<float> add(const TensorView<float> &a, const TensorView<float> &b);

    /**
     * @brief Calculates (a + b) * scale
     *
     * @param a         [...] float tensor, input a
     * @param b         [...] float tensor, input b
     * @param scale     Scale for second tensor
     * @returns         (a + b) * scale
     */
    Tensor<float> add_scale(const TensorView<float> &a, const TensorView<float> &b, const float b_scale);

    /**
     * @brief Calculates a + (b * scale)
     *
     * @param a         [...] float tensor, input a
     * @param b         [...] float tensor, input b
     * @param scale     Scale for second tensor
     * @returns         a + b * scale
     */
    Tensor<float> add_scale_b(const TensorView<float> &a, const TensorView<float> &b, const float b_scale);

} // tlib::ops

#endif