#ifndef CONFORMER_FEEDFORWARD_H_
#define CONFORMER_FEEDFORWARD_H_

#include <tlib_tensor.h>
#include <conformer_tensor_ids.h>

namespace conformer
{

    /**
     * Forward pass through the feed-forward module of the conformer.
     *
     * @param   x               [n, m] float tensor, input embeddings
     * @param   weight0         Parameter ID of weight for first linear layer
     * @param   weight1         Parameter ID of weight for second linear layer
     * @param   bias0           Parameter ID of bias for first linear layer
     * @param   bias1           Parameter ID of bias for second linear layer
     * @param   input0_shift    Quantization shift of input for first linear layer
     * @param   input1_shift    Quantization shift of input for second linear layer
     * 
     * @return  [n, m] float tensor, output embeddings
     */
    tlib::Tensor<float> FeedForward(
        const tlib::TensorView<float> &x,
        const TensorID weight0,
        const TensorID weight1,
        const TensorID bias0,
        const TensorID bias1,
        const uint8_t input0_shift,
        const uint8_t input1_shift);

} // conformer

#endif