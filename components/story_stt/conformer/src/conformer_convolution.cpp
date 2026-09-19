#include "conformer_convolution.h"

#include <array>

#include <conformer.h>
#include <conformer_tensor_ids.h>
#include <tlib_flash.h>
#include <tlib_ops.h>

namespace conformer
{

    static constexpr const TensorID pointwise_conv1_weight_base{ENCODER_LAYERS_0_CONV_POINTWISE1_WEIGHT_Q_INT8};
    static constexpr const TensorID pointwise_conv1_bias_base{ENCODER_LAYERS_0_CONV_POINTWISE1_BIAS_Q_INT32};
    static constexpr const TensorID depthwise_conv_weight_base{ENCODER_LAYERS_0_CONV_DEPTHWISE_CONV_WEIGHT_Q_INT8};
    static constexpr const TensorID depthwise_conv_bias_base{ENCODER_LAYERS_0_CONV_DEPTHWISE_CONV_BIAS_Q_INT32};
    static constexpr const TensorID batchnorm_weight_base{ENCODER_LAYERS_0_CONV_BATCH_NORM_WEIGHT_FLOAT32};
    static constexpr const TensorID batchnorm_bias_base{ENCODER_LAYERS_0_CONV_BATCH_NORM_BIAS_FLOAT32};
    static constexpr const TensorID batchnorm_running_mean_base{ENCODER_LAYERS_0_CONV_BATCH_NORM_RUNNING_MEAN_FLOAT32};
    static constexpr const TensorID batchnorm_running_var_base{ENCODER_LAYERS_0_CONV_BATCH_NORM_RUNNING_VAR_FLOAT32};
    static constexpr const TensorID pointwise_conv2_weight_base{ENCODER_LAYERS_0_CONV_POINTWISE2_WEIGHT_Q_INT8};
    static constexpr const TensorID pointwise_conv2_bias_base{ENCODER_LAYERS_0_CONV_POINTWISE2_BIAS_Q_INT32};

    // Quantization shifts for inputs of linear/conv layers in convolution module
    static constexpr const std::array<uint8_t, kLayers> pointwise_conv1_input_shift{7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
    static constexpr const std::array<uint8_t, kLayers> depthwise_conv_input_shift{3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3};
    static constexpr const std::array<uint8_t, kLayers> pointwise_conv2_input_shift{2, 6, 6, 6, 5, 5, 6, 5, 5, 6, 6, 6, 6, 6, 6, 5};

    tlib::Tensor<float> Convolution(const tlib::TensorView<float> &x, const uint32_t layer_index)
    {

        tlib::Tensor<float> y;

        // pointwise_conv1
        {
            const auto weight = tlib::flash::LoadTensor<int8_t>(GetLayerTensorID(pointwise_conv1_weight_base, layer_index), tlib::heap::Type::SRAM);
            const auto bias = tlib::flash::LoadTensor<int32_t>(GetLayerTensorID(pointwise_conv1_bias_base, layer_index), tlib::heap::Type::SRAM);
            auto x_q = tlib::ops::quantize(x, pointwise_conv1_input_shift.at(layer_index));
            y = tlib::ops::linear_deq(x_q, weight, bias);
        }

        // x = glu(x).transpose(0, 1)
        y = tlib::ops::glu(y);

        // depthwise_conv
        {
            const auto weight = tlib::flash::LoadTensor<int8_t>(GetLayerTensorID(depthwise_conv_weight_base, layer_index), tlib::heap::Type::SRAM);
            const auto bias = tlib::flash::LoadTensor<int32_t>(GetLayerTensorID(depthwise_conv_bias_base, layer_index), tlib::heap::Type::SRAM);
            auto y_q = tlib::ops::quantize(y, depthwise_conv_input_shift.at(layer_index), tlib::heap::Type::SRAM);
            y = tlib::ops::conv1d_dw_deq(y_q, weight, bias);
        }

        // batch_norm
        {
            const auto weight = tlib::flash::LoadTensor<float>(GetLayerTensorID(batchnorm_weight_base, layer_index), tlib::heap::Type::SRAM);
            const auto bias = tlib::flash::LoadTensor<float>(GetLayerTensorID(batchnorm_bias_base, layer_index), tlib::heap::Type::SRAM);
            const auto running_mean = tlib::flash::LoadTensor<float>(GetLayerTensorID(batchnorm_running_mean_base, layer_index), tlib::heap::Type::SRAM);
            const auto running_var = tlib::flash::LoadTensor<float>(GetLayerTensorID(batchnorm_running_var_base, layer_index), tlib::heap::Type::SRAM);
            y = tlib::ops::batchnorm(y, weight, bias, running_mean, running_var);
        }

        y = tlib::ops::transpose(y, 0, 1);

        // relu
        y = tlib::ops::relu(y);

        // pointwise_conv2
        {
            const auto weight = tlib::flash::LoadTensor<int8_t>(GetLayerTensorID(pointwise_conv2_weight_base, layer_index), tlib::heap::Type::SRAM);
            const auto bias = tlib::flash::LoadTensor<int32_t>(GetLayerTensorID(pointwise_conv2_bias_base, layer_index), tlib::heap::Type::SRAM);
            auto y_q = tlib::ops::quantize(y, pointwise_conv2_input_shift.at(layer_index));
            y = tlib::ops::linear_deq(y_q, weight, bias);
        }

        return y;
    }

} // conformer