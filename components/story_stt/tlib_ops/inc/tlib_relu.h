#ifndef TLIB_RELU_H_
#define TLIB_RELU_H_

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * @brief Performs the relu activation function.
     *
     * @param x     [...] float tensor, input
     *
     * @returns     [...] float tensor, output
     */
    Tensor<float> relu(const TensorView<float> &x);

} // tlib::ops

#endif