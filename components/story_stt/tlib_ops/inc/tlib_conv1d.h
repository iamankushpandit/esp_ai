#ifndef TLIB_CONV1D_H_
#define TLIB_CONV1D_H_

#include <cstdint>
#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Performs a 1D depthwise convolution over input x with weight w and bias b,
     * assuming stride=1, dilation=1, padding="same" and groups=in_channels. The
     * int32 matrix multiplication result is dequantized before being stored into
     * the output tensor.
     *
     * ic       input channels (= output channels for depthwise convolution)
     * in_len   input length
     * k        kernel size
     *
     * @param x     [ic, in_len] int8_t tensor, input
     * @param w     [ic, 1, k] int8_t tensor, weight
     * @param b     [ic] int32_t tensor, bias
     *
     * @returns     [ic, in_len] float tensor, output
     */
    Tensor<float> conv1d_dw_deq(const TensorView<int8_t> &x, const TensorView<int8_t> &w, const TensorView<int32_t> &b);

} // tlib::ops

#endif