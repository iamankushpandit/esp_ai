#ifndef TLIB_CONV2D_H_
#define TLIB_CONV2D_H_

#include <cstdint>
#include <tlib_tensor.h>

namespace tlib::ops
{
    /**
     * Performs a 2D convolution over input x with weight w and bias b, assuming groups=1
     * and padding='zeros'. The int32_t result of the kernel multiplication is shifted such that
     * the result has y_shift output quantization shift.
     * After shifting, the result is clamped to the range [0, 127] and converted to int8_t.
     *
     * @param x         [ic, n, m] int8 input tensor
     * @param w         [oc, ic*k*k] int8_t tensor, weight (4D tensor converted via w.view(w.shape[0], -1))
     * @param b         [oc] int32 bias tensor
     * @param oc        Number of output channels
     * @param kh        Kernel height
     * @param kw        Kernel width
     * @param sh        Stride for height dimension
     * @param sw        Stride for width dimension
     * @param ph        Padding for height dimension
     * @param pw        Padding for width dimension
     * @param dh        Dilation for height dimension
     * @param dw        Dilation for width dimension
     * @param y_shift   Desired output shift, must be in range [0, 7]
     *
     * @returns     relu(conv2d(x, w, b)) >> y_shift - b_shift
     */
    Tensor<int8_t> conv2d_relu(const TensorView<int8_t> &x, const TensorView<int8_t> &w, const TensorView<int32_t> &b,
                               const uint8_t oc, const uint8_t kh, const uint8_t kw, const uint8_t sh, const uint8_t sw,
                               const uint8_t ph, const uint8_t pw, const uint8_t dh, const uint8_t dw, const uint8_t y_shift);

    /**
     * Performs a 2D convolution over input x with weight w, assuming groups=1
     * and padding='zeros'. The int32_t result of the kernel multiplication is shifted such that
     * the result has y_shift output quantization shift.
     * After shifting, the result is clamped to the range [0, 127] and converted to int8_t.
     *
     * @param x         [ic, n, m] int8 input tensor
     * @param w         [oc, ic*k*k] int8_t tensor, weight (4D tensor converted via w.view(w.shape[0], -1))
     * @param oc        Number of output channels
     * @param kh        Kernel height
     * @param kw        Kernel width
     * @param sh        Stride for height dimension
     * @param sw        Stride for width dimension
     * @param ph        Padding for height dimension
     * @param pw        Padding for width dimension
     * @param dh        Dilation for height dimension
     * @param dw        Dilation for width dimension
     * @param y_shift   Desired output shift, must be in range [0, 7]
     *
     * @returns     relu(conv2d(x, w)) >> y_shift - x_shift - w_shift
     */
    Tensor<int8_t> conv2d_relu(const TensorView<int8_t> &x, const TensorView<int8_t> &w,
                               const uint8_t oc, const uint8_t kh, const uint8_t kw, const uint8_t sh, const uint8_t sw,
                               const uint8_t ph, const uint8_t pw, const uint8_t dh, const uint8_t dw, const uint8_t y_shift);

} // tlib::ops

#endif