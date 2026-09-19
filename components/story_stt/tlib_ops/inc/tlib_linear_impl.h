#ifndef TLIB_LINEAR_IMPL_H_
#define TLIB_LINEAR_IMPL_H_

#include <cstdint>

namespace tlib::ops::impl {

void linear_b_relu_impl_lshift(const int8_t *a, const int8_t *b, const int32_t *c, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t lshift);
void linear_b_relu_impl_rshift(const int8_t *a, const int8_t *b, const int32_t *c, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t rshift);
void linear_relu_impl_lshift(const int8_t *a, const int8_t *b, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t lshift);
void linear_relu_impl_rshift(const int8_t *a, const int8_t *b, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t rshift);
void linear_b_deq_impl(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t shift);
void linear_deq_impl(const int8_t *a, const int8_t *b, float *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t shift);
void linear_b_relu_deq_impl(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t shift);
void linear_impl(const float *a, const float*b, float *y, const uint32_t k, const uint32_t n, const uint32_t m);

} // tlib::ops::impl

#endif