#ifndef CONFORMER_LAYER_H_
#define CONFORMER_LAYER_H_

#include <tlib_tensor.h>

namespace conformer
{

    /**
     * Forward pass through a complete conformer layer.
     *
     * @param x             [n, m] float tensor, input embeddings
     * @param pos_emb       [2*n, m] int8_t tensor, position embeddings
     * @param layer_index   Index of conformer layer, must be in range [0, 15]
     *
     * @returns             [n, m] float tensor, output embeddings
     */
    tlib::Tensor<float> Layer(const tlib::TensorView<float> &x, const tlib::Tensor<int8_t> &pos_emb, const uint16_t layer_index);

} // conformer

#endif