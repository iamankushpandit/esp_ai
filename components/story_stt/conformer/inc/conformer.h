#ifndef CONFORMER_H_
#define CONFORMER_H_

#include <tlib_tensor.h>

namespace conformer {

// Embedding dimension of the conformer model
inline constexpr const uint32_t kEmbeddingDimension{176};
// Number of conformer layers
inline constexpr const uint16_t kLayers{16};

/**
 * Forward pass through the conformer.
 * 
 * @param   x   [n, m] float tensor, input embeddings (output of PreEncode)
 * 
 * @returns [n, n_vocab] float tensor, logits
 */
tlib::Tensor<float> Forward(const tlib::TensorView<float> & pre_enc);

} // conformer

#endif