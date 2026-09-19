#include "conformer_pre_encode.h"

#include <conformer_tensor_ids.h>
#include <tlib_ops.h>
#include <tlib_flash.h>

namespace conformer
{

    // Inputs shifts for convolution layers in pre-encode module
    constexpr const uint8_t kConv0InputShift{4};
    constexpr const uint8_t kConv0OutputShift{6};
    constexpr const uint8_t kConv1OutputShift{1};

    tlib::Tensor<float> PreEncode(const tlib::TensorView<float> &log_mel)
    {
        auto x = tlib::ops::quantize(log_mel, kConv0InputShift);
        x.Unsqueeze(0);

        // Conv0
        // Compared to the original conformer, the kernel size is zero-padded from 3x3 to 4x4.
        // This makes the inner dimension for the underlying matrix multiplication divisable by 16.
        {
            auto w = tlib::flash::LoadTensor<int8_t>(conformer::TensorID::ENCODER_PRE_ENCODE_CONV0_WEIGHT_Q_INT8, tlib::heap::Type::SRAM);
            auto b = tlib::flash::LoadTensor<int32_t>(conformer::TensorID::ENCODER_PRE_ENCODE_CONV0_BIAS_Q_INT32, tlib::heap::Type::SRAM);
            w.View({176, 16});
            x = tlib::ops::conv2d_relu(x, w, b, 176, 4, 4, 2, 2, 1, 1, 1, 1, kConv0OutputShift);
        }

        // Conv1
        // Compared to the original conformer, the bias was removed.
        // This makes it possible to perform the underlying matrix multiplication as transposed, which
        // means x in SRAM and weight mapped from flash
        {
            auto w = tlib::flash::MapTensor<int8_t>(conformer::TensorID::ENCODER_PRE_ENCODE_CONV1_WEIGHT_Q_INT8);
            w.View({176, 9 * 176});
            x = tlib::ops::conv2d_relu(x, w, 176, 3, 3, 2, 2, 1, 1, 1, 1, kConv1OutputShift);
        }

        // [176, n_seq/4, d_mel/4] -> [n_seq/4, 176, d_mel/4]
        x = tlib::ops::transpose(x, 0, 1, tlib::heap::Type::SRAM);

        // [n_seq/4, 176, d_mel/4] -> [n_seq/4, 176*d_mel/4]
        x.Flatten(1, 2);

        // Linear
        // Compared to the original conformer, the bias was removed. Same reason as for Conv1
        auto w = tlib::flash::MapTensor<int8_t>(conformer::TensorID::ENCODER_PRE_ENCODE_OUT_WEIGHT_Q_INT8);
        auto y_T = tlib::ops::linear_deq(w, x);
        return tlib::ops::transpose(y_T, 0, 1);
    }

} // conformer