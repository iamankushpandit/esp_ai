#ifndef TLIB_BMM_H_
#define TLIB_BMM_H_

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Performs a batched matrix multiplication, y[i] = a[i]*b[i]^T
     * 
     * @param   [p, n, k] float tensor, input a
     * @param   [p, m, k] float tensor, input b
     *
     * @return  [p, n, m] float tensor, output
     */
    Tensor<float> bmm(const TensorView<float> &a, const TensorView<float> &b);

} // tlib::ops

#endif