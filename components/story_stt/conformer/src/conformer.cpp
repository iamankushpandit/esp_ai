#include "conformer.h"

#include <print>

#include <conformer_tensor_ids.h>
#include <conformer_pos_encode.h>
#include <conformer_layer.h>

#include <tlib_ops.h>
#include <tlib_flash.h>

namespace conformer
{

    // Input shift for linear layer of the decoder
    static constexpr const uint8_t kDecoderInputShift{5};

    tlib::Tensor<float> Forward(const tlib::TensorView<float> &pre_enc)
    {
        auto [x, pos_enc] = PosEncode(pre_enc);

        for (uint16_t i = 0; i < kLayers; i++)
        {
            x = Layer(x, pos_enc, i);
        }

        auto x_q = tlib::ops::quantize(x, kDecoderInputShift);

        x = tlib::ops::linear_deq(
            x_q,
            tlib::flash::LoadTensor<int8_t>(DECODER_LINEAR_WEIGHT_Q_INT8, tlib::heap::Type::SRAM),
            tlib::flash::LoadTensor<int32_t>(DECODER_LINEAR_BIAS_Q_INT32, tlib::heap::Type::SRAM));

        return x;
    }

} // conformer