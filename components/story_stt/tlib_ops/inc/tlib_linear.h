#ifndef TLIB_LINEAR_H_
#define TLIB_LINEAR_H_

#include <cstdint>

#include <tlib_tensor.h>

namespace tlib::ops {

/**
 * @brief Calculates y = relu(x*w^T + b). The int32 matrix multiplication result is
 * afterwards shifted such that the result has y_shift output quantization shift. 
 * After shifting, the result is clamped to the range [0, 127] and converted to int8.
 * 
 * @param x         [n, k] int8_t tensor, input
 * @param w         [m, k] int8_t tensor, weights
 * @param b         [m] int32_t tensor, bias
 * @param y_shift   Desired output shift, must be in range [0, 7]
 * 
 * @returns     [n, m] int8_t tensor, relu(x*w^T + b) >> (y_shift - b_shift)
 */
Tensor<int8_t> linear_relu(const TensorView<int8_t> & x, const TensorView<int8_t> & w, const TensorView<int32_t> & b, const uint8_t y_shift);

/**
 * @brief Calculates y = relu(x*w^T). The int32 matrix multiplication result is
 * afterwards shifted such that the result has y_shift output quantization shift. 
 * After shifting, the result is clamped to the range [0, 127] and converted to int8.
 * 
 * @param x         [n, k] int8_t tensor, input
 * @param w         [m, k] int8_t tensor, weights
 * @param y_shift   Desired output shift, must be in range [0, 7]
 * 
 * @returns     [n, m] int8_t tensor, relu(x*w^T) >> (y_shift - x_shift - w_shift)
 */
Tensor<int8_t> linear_relu(const TensorView<int8_t> & x, const TensorView<int8_t> & w, const uint8_t y_shift);

/**
 * @brief Calculates y = dequantize(x*w^T + b). The int32 matrix multiplication result is
 * dequantized before being stored into the output tensor.
 * 
 * @param x         [n, k] int8_t tensor, input
 * @param w         [m, k] int8_t tensor, weights
 * @param b         [m] int32_t tensor, bias
 * 
 * @returns     [n, m] float tensor, dequantize(x*w^T + b)
 */
Tensor<float> linear_deq(const TensorView<int8_t> & x, const TensorView<int8_t> & w, const TensorView<int32_t> & b);

/**
 * @brief Calculates y = dequantize(x*w^T). The int32 matrix multiplication result is
 * dequantized before being stored into the output tensor.
 * 
 * @param x         [n, k] int8_t tensor, input
 * @param w         [m, k] int8_t tensor, weights
 * 
 * @returns     [n, m] float tensor, dequantize(x*w^T)
 */
Tensor<float> linear_deq(const TensorView<int8_t> & x, const TensorView<int8_t> & w);

/**
 * @brief Calculates y = dequantize(relu(x*w^T + b)). The int32 matrix multiplication 
 * result is dequantized before being stored into the output tensor.
 * 
 * @param x         [n, k] int8_t tensor, input
 * @param w         [m, k] int8_t tensor, weights
 * @param b         [m] int32_t tensor, bias
 * 
 * @returns     [n, m] float tensor, dequantize(relu(x*w^T + b))
 */
Tensor<float> linear_relu_deq(const TensorView<int8_t> & x, const TensorView<int8_t> & w, const TensorView<int32_t> & b);

/**
 * @brief Calculates y = x*w^T.
 * 
 * @param x         [n, k] float tensor, input
 * @param w         [m, k] float tensor, weights
 * 
 * @returns     [n, m] float tensor, x*w^T
 */
Tensor<float> linear(const TensorView<float> & x, const TensorView<float> & w);

} // tlib::ops

#endif