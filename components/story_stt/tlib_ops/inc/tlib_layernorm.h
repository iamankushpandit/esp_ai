#ifndef TLIB_LAYERNORM_H_
#define TLIB_LAYERNORM_H_

#include <cstdint>

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Performs the layernorm operation.
     *
     * @param   x       [n, m] float tensor, input
     * @param   weights [m] float tensor, weights
     * @param   bias    [m] float tensor, bias
     *
     * @return  [n, m] float tensor, output
     */
    Tensor<float> layernorm(const TensorView<float> &x, const TensorView<float> &weights, const TensorView<float> &bias);

} // tlib::ops

#endif