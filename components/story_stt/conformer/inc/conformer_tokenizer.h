#ifndef CONFORMER_TOKENIZER_H_
#define CONFORMER_TOKENIZER_H_

#include <tlib_tensor.h>

namespace conformer {

/**
 * Decodes a sequence of tokens into a text buffer.
 * 
 * @param   tokens  [n] uint32_t tensor, argmax of the output logits
 * @param   buffer  Buffer to which decoded text is appended
 */
void Decode(const tlib::TensorView<uint32_t> & tokens, std::string & buffer);

} // conformer

#endif