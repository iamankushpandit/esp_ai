#ifndef CONFORMER_PRE_ENCODE_H_
#define CONFORMER_PRE_ENCODE_H_

#include <tlib_tensor.h>

namespace conformer
{

    /**
     * Forward pass through the pre-encode module of the conformer.
     *
     * @param log_mel   [n, m] float tensor, log-mel spectrogram
     *
     * @returns         [p, n/4] float tensor, output embeddings
     */
    tlib::Tensor<float> PreEncode(const tlib::TensorView<float> &log_mel);

} // conformer

#endif