#ifndef TLIB_SOFTMAX_H_
#define TLIB_SOFTMAX_H_

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Computes the softmax along the last dimension.
     *
     * @param   x   [p, n, m] float tensor, input
     *
     * @return  [p, n, m] float tensor, output
     */
    Tensor<float> softmax(const TensorView<float> &x);

} // tlib::ops

#endif