#ifndef CONFORMER_POS_ENCODE_H_
#define CONFORMER_POS_ENCODE_H_

#include <tuple>

#include <tlib_tensor.h>

namespace conformer
{

    /**
     * Forward pass through the positional encoding module of the conformer.
     *
     * @param   x       [n, m] float tensor, input embeddings
     *
     * @return  y       [n, m] float tensor, output embeddings
     *          pos_enc [2*n, m] int8_t tensor, quantized position encodings
     */
    std::tuple<tlib::Tensor<float>, tlib::Tensor<int8_t>> PosEncode(const tlib::TensorView<float> &x);

} // conformer

#endif