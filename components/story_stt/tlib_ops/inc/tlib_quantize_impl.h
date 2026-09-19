#ifndef TLIB_QUANTIZE_IMPL_H_
#define TLIB_QUANTIZE_IMPL_H_

#include <cstdint>

namespace tlib::ops::impl {

void quantize_impl(const float *a, int8_t *a_q, uint32_t n, uint8_t shift);

} // tlib::ops::impl

#endif