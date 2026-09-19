#ifndef TLIB_ARGMAX_H_
#define TLIB_ARGMAX_H_

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Argmax along the last dimension
     *
     * @param   [n, m] float tensor, input
     *
     * @returns [n] uint32_t tensor, output
     */
    Tensor<uint32_t> argmax(const TensorView<float> &x);

} // tlib::ops

#endif