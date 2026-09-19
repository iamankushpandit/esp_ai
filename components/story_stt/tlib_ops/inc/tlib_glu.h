#ifndef TLIB_GLU_H_
#define TLIB_GLU_H_

#include <cstdint>

#include <tlib_tensor.h>

namespace tlib::ops
{

    /**
     * Gated linear unit function
     *
     * @param   x   [n, m] float tensor, input
     *
     * @returns     [n/2, m] float tensor, output
     */
    Tensor<float> glu(const Tensor<float> &x);

} // tlib::ops

#endif