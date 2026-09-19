#ifndef TLIB_BATCHNORM_H_
#define TLIB_BATCHNORM_H_

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * @brief Performs the batchnorm operation.
     *
     * @param x             [n, m] float tensor, input
     * @param w             [n] float tensor, weight
     * @param b             [n] float tensor, bias
     * @param running_mean  [n] float tensor, running mean
     * @param running_var   [n] float tensor, running var
     *
     * @returns [n, m] float tensor, output
     */
    Tensor<float> batchnorm(const TensorView<float> &x, const TensorView<float> &w, const TensorView<float> &b,
                            const TensorView<float> &running_mean, const TensorView<float> &running_var);

} // tlib::ops

#endif