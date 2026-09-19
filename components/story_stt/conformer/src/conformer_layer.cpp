#include "conformer_layer.h"

#include <conformer.h>
#include <conformer_feedforward.h>
#include <conformer_attention.h>
#include <conformer_convolution.h>
#include <conformer_tensor_ids.h>
#include <tlib_flash.h>
#include <tlib_ops.h>

namespace conformer
{

    static constexpr const TensorID ff1_weight1_base{ENCODER_LAYERS_0_FEED_FORWARD1_LINEAR1_RELU_WEIGHT_Q_INT8};
    static constexpr const TensorID ff1_weight2_base{ENCODER_LAYERS_0_FEED_FORWARD1_LINEAR2_WEIGHT_Q_INT8};
    static constexpr const TensorID ff1_bias1_base{ENCODER_LAYERS_0_FEED_FORWARD1_LINEAR1_RELU_BIAS_Q_INT32};
    static constexpr const TensorID ff1_bias2_base{ENCODER_LAYERS_0_FEED_FORWARD1_LINEAR2_BIAS_Q_INT32};
    static constexpr const TensorID ff2_weight1_base{ENCODER_LAYERS_0_FEED_FORWARD2_LINEAR1_RELU_WEIGHT_Q_INT8};
    static constexpr const TensorID ff2_weight2_base{ENCODER_LAYERS_0_FEED_FORWARD2_LINEAR2_WEIGHT_Q_INT8};
    static constexpr const TensorID ff2_bias1_base{ENCODER_LAYERS_0_FEED_FORWARD2_LINEAR1_RELU_BIAS_Q_INT32};
    static constexpr const TensorID ff2_bias2_base{ENCODER_LAYERS_0_FEED_FORWARD2_LINEAR2_BIAS_Q_INT32};
    static constexpr const TensorID norm_ff1_weight_base{ENCODER_LAYERS_0_NORM_FEED_FORWARD1_WEIGHT_FLOAT32};
    static constexpr const TensorID norm_ff1_bias_base{ENCODER_LAYERS_0_NORM_FEED_FORWARD1_BIAS_FLOAT32};
    static constexpr const TensorID norm_attn_weight_base{ENCODER_LAYERS_0_NORM_SELF_ATT_WEIGHT_FLOAT32};
    static constexpr const TensorID norm_attn_bias_base{ENCODER_LAYERS_0_NORM_SELF_ATT_BIAS_FLOAT32};
    static constexpr const TensorID norm_conv_weight_base{ENCODER_LAYERS_0_NORM_CONV_WEIGHT_FLOAT32};
    static constexpr const TensorID norm_conv_bias_base{ENCODER_LAYERS_0_NORM_CONV_BIAS_FLOAT32};
    static constexpr const TensorID norm_ff2_weight_base{ENCODER_LAYERS_0_NORM_FEED_FORWARD2_WEIGHT_FLOAT32};
    static constexpr const TensorID norm_ff2_bias_base{ENCODER_LAYERS_0_NORM_FEED_FORWARD2_BIAS_FLOAT32};
    static constexpr const TensorID norm_out_weight_base{ENCODER_LAYERS_0_NORM_OUT_WEIGHT_FLOAT32};
    static constexpr const TensorID norm_out_bias_base{ENCODER_LAYERS_0_NORM_OUT_BIAS_FLOAT32};

    // Input shifts for linear layers of feed forward modules
    static constexpr const std::array<uint8_t, kLayers> ff1_input1_shift{2, 7, 7, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 7};
    static constexpr const std::array<uint8_t, kLayers> ff1_input2_shift{0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
    static constexpr const std::array<uint8_t, kLayers> ff2_input1_shift{3, 7, 6, 7, 6, 6, 6, 6, 6, 5, 5, 5, 6, 6, 7, 7};
    static constexpr const std::array<uint8_t, kLayers> ff2_input2_shift{0, 6, 6, 6, 6, 5, 6, 5, 5, 6, 6, 6, 6, 6, 6, 5};

    static constexpr const float kFeedForwardScale{0.5f};

    tlib::Tensor<float> Layer(const tlib::TensorView<float> &x, const tlib::Tensor<int8_t> &pos_emb, const uint16_t layer_index)
    {
        tlib::Tensor<float> residual;

        {
            auto z = tlib::ops::layernorm(
                x,
                tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_ff1_weight_base, layer_index), tlib::heap::Type::SRAM),
                tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_ff1_bias_base, layer_index), tlib::heap::Type::SRAM));
            z = FeedForward(
                z,
                GetLayerTensorID(ff1_weight1_base, layer_index),
                GetLayerTensorID(ff1_weight2_base, layer_index),
                GetLayerTensorID(ff1_bias1_base, layer_index),
                GetLayerTensorID(ff1_bias2_base, layer_index),
                ff1_input1_shift.at(layer_index),
                ff1_input2_shift.at(layer_index));
            residual = tlib::ops::add_scale_b(x, z, kFeedForwardScale);
        }

        {
            auto z = tlib::ops::layernorm(
                residual,
                tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_attn_weight_base, layer_index), tlib::heap::Type::SRAM),
                tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_attn_bias_base, layer_index), tlib::heap::Type::SRAM));
            z = Attention(z, pos_emb, layer_index);
            residual = tlib::ops::add(residual, z);
        }

        {
            auto z = tlib::ops::layernorm(
                residual,
                tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_conv_weight_base, layer_index), tlib::heap::Type::SRAM),
                tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_conv_bias_base, layer_index), tlib::heap::Type::SRAM));
            z = Convolution(z, layer_index);
            residual = tlib::ops::add(residual, z);
        }

        {
            auto z = tlib::ops::layernorm(
                residual,
                tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_ff2_weight_base, layer_index), tlib::heap::Type::SRAM),
                tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_ff2_bias_base, layer_index), tlib::heap::Type::SRAM));
            z = FeedForward(
                z,
                GetLayerTensorID(ff2_weight1_base, layer_index),
                GetLayerTensorID(ff2_weight2_base, layer_index),
                GetLayerTensorID(ff2_bias1_base, layer_index),
                GetLayerTensorID(ff2_bias2_base, layer_index),
                ff2_input1_shift.at(layer_index),
                ff2_input2_shift.at(layer_index));
            residual = tlib::ops::add_scale_b(residual, z, kFeedForwardScale);
        }

        return tlib::ops::layernorm(
            residual,
            tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_out_weight_base, layer_index), tlib::heap::Type::SRAM),
            tlib::flash::LoadTensor<float>(GetLayerTensorID(norm_out_bias_base, layer_index), tlib::heap::Type::SRAM));
    }

} // conformer