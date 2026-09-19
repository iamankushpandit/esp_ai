#ifndef TLIB_QUANTIZE_H_
#define TLIB_QUANTIZE_H_

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Quantizes the given tensor with the given shift as:
     *
     * y[i] = clamp(round(x[i] >> shift), -128, 127)
     *
     * @param   x           [...] float tensor, input
     * @param   shift       Quantization shift to apply
     * @param   location    Heap on which to allocate output buffer
     *
     * @return  int8_t tensor, quantization result
     */
    Tensor<int8_t> quantize(const TensorView<float> &x, const uint8_t shift, const heap::Type location = heap::kDefaultLocation);

    /**
     * Dequantizes the given quantized tensor as:
     *
     * y[i] = x[i] << x.Shift()
     *
     * @param   x   [...] int8_t tensor, quantized input
     *
     * @return  float tensor, dequantized result
     */
    Tensor<float> dequantize(const TensorView<int8_t> &x);

} // tlib::ops

#endif