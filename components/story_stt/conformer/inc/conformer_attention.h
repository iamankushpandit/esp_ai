#ifndef CONFORMER_ATTENTION_H_
#define CONFORMER_ATTENTION_H_

#include <tlib_tensor.h>

namespace conformer
{

    /**
     * Forward pass through the self-attention module of the conformer.
     *
     * @param   x           [n, m] float tensor, input embeddings
     * @param   pos_emb     [2*n, m] int8_t tensor, position embeddings
     * @param   layer_index Index of the layer for which to evaluate the attention module
     *
     * @return  [n, m] float tensor, output embeddings
     */
    tlib::Tensor<float> Attention(const tlib::TensorView<float> &x, const tlib::TensorView<int8_t> &pos_emb, const uint32_t layer_index);

} // conformer

#endif