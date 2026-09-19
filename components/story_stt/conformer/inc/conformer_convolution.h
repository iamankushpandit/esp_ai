#ifndef CONFORMER_CONVOLUTION_H_
#define CONFORMER_CONVOLUTION_H_

#include <tlib_tensor.h>

namespace conformer
{

    /**
     * Forward pass through convolution module of the conformer.
     *
     * @param   x           [n, m] float tensor, input embeddings
     * @param   layer_index Index of the layer for which to evaluate the convolution module
     *
     * @return  [n, m] float tensor, output embeddings
     */
    tlib::Tensor<float> Convolution(const tlib::TensorView<float> &x, const uint32_t layer_index);

} // conformer

#endif