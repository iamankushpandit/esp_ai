#include "conformer_attention.h"

#include <math.h>

#include <conformer.h>
#include <conformer_tensor_ids.h>

#include <tlib_flash.h>
#include <tlib_ops.h>

namespace conformer
{

    static constexpr const TensorID linear_q_weight_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_Q_WEIGHT_Q_INT8};
    static constexpr const TensorID linear_q_bias_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_Q_BIAS_Q_INT32};
    static constexpr const TensorID linear_k_weight_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_K_WEIGHT_Q_INT8};
    static constexpr const TensorID linear_k_bias_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_K_BIAS_Q_INT32};
    static constexpr const TensorID linear_v_weight_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_V_WEIGHT_Q_INT8};
    static constexpr const TensorID linear_v_bias_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_V_BIAS_Q_INT32};
    static constexpr const TensorID linear_out_weight_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_OUT_WEIGHT_Q_INT8};
    static constexpr const TensorID linear_out_bias_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_OUT_BIAS_Q_INT32};
    static constexpr const TensorID linear_pos_weight_base{ENCODER_LAYERS_0_SELF_ATTN_LINEAR_POS_WEIGHT_Q_INT8};
    static constexpr const TensorID linear_pos_bias_u_base{ENCODER_LAYERS_0_SELF_ATTN_POS_BIAS_U_FLOAT32};
    static constexpr const TensorID linear_pos_bias_v_base{ENCODER_LAYERS_0_SELF_ATTN_POS_BIAS_V_FLOAT32};

    // Quantization shifts for inputs of linear layers in attention module
    static constexpr const std::array<uint8_t, kLayers> linear_q_input_shift{7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
    static constexpr const std::array<uint8_t, kLayers> linear_k_input_shift{7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
    static constexpr const std::array<uint8_t, kLayers> linear_v_input_shift{7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
    static constexpr const std::array<uint8_t, kLayers> linear_out_input_shift{2, 6, 6, 7, 7, 6, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6};

    static constexpr const uint32_t kNumHeads{4};
    static constexpr const uint32_t kHeadDimension{kEmbeddingDimension / 4};
    static constexpr const float kScale{0.15075567228888181f}; /*!< 1.0 / sqrt(kHeadDimension) */
    static_assert(kEmbeddingDimension % kNumHeads == 0);

    /**
     * Performs the skewing operation for relative position encoding.
     * 2D example for clarity:
     *
     *  1   2   3   4   5   6               3   4   5   6   0   7
     *  7   8   9   10  11  12      ->      8   9   10  11  12  0
     *  13  14  15  16  17  18              13  14  15  16  17  18
     *
     * @param   x [p, n, m] float tensor, input
     *
     * @returns [p, n, m] float tensor, skewed output
     */
    static tlib::Tensor<float> RelativePositionalShift(const tlib::TensorView<float> &x)
    {
        // This implementation performs the relative positional shift by doing
        // the following for each page in the input tensor x of shape [p, n, m]:
        //
        //  1. Skip the first n - 1 elements of x
        //  2. Copy the next m - n + 1 elements of x to y
        //  3. Repeat the following for n - 1 times:
        //  3.1.    Write 0 to the next element of y
        //  3.2.    Copy the next m elements of x to y

        assert(x.Dim() == 3);
        assert(x.Shape(1) > 1);
        assert(x.Shape(2) >= x.Shape(1));

        auto y = tlib::Tensor<float>(x.Shape(), false, 0);

        const uint32_t n_iter{x.Shape(1) - 1};
        const uint32_t n_cpy{x.Shape(2)};
        const uint32_t n_cpy_init{x.Shape(2) - x.Shape(1) + 1};
        const float *x_data{x.DataImm()};
        float *y_data{y.Data()};

        for (uint32_t p = 0; p < x.Shape(0); p++)
        {
            x_data += x.Shape(1) - 1;

            std::memcpy(y_data, x_data, sizeof(float) * n_cpy_init);
            y_data += n_cpy_init;
            x_data += n_cpy_init;

            for (uint32_t i = 0; i < n_iter; i++)
            {
                *y_data = 0.0f;
                y_data += 1;

                std::memcpy(y_data, x_data, sizeof(float) * n_cpy);
                x_data += n_cpy;
                y_data += n_cpy;
            }
        }

        return y;
    }

    tlib::Tensor<float> Attention(const tlib::TensorView<float> &x, const tlib::TensorView<int8_t> &pos_emb, const uint32_t layer_index)
    {
        auto x_q = tlib::ops::quantize(x, linear_q_input_shift.at(layer_index));

        // q_with_bias_u and q_with_bias_v
        tlib::Tensor<float> q_with_bias_u, q_with_bias_v;
        {
            auto query = tlib::ops::linear_deq(
                x_q,
                tlib::flash::LoadTensor<int8_t>(GetLayerTensorID(linear_q_weight_base, layer_index), tlib::heap::Type::SRAM),
                tlib::flash::LoadTensor<int32_t>(GetLayerTensorID(linear_q_bias_base, layer_index), tlib::heap::Type::SRAM));
            query.View({query.Shape(0), kNumHeads, kHeadDimension});

            q_with_bias_u = tlib::Tensor<float>(query.Shape(), false, 0);
            q_with_bias_v = tlib::Tensor<float>(query.Shape(), false, 0);
            {
                auto bias_u = tlib::flash::LoadTensor<float>(GetLayerTensorID(linear_pos_bias_u_base, layer_index), tlib::heap::Type::SRAM);
                auto bias_v = tlib::flash::LoadTensor<float>(GetLayerTensorID(linear_pos_bias_v_base, layer_index), tlib::heap::Type::SRAM);
                assert(bias_u.Dim() == 2);
                assert(bias_v.Dim() == 2);
                assert(bias_u.Shape(0) == q_with_bias_u.Shape(1));
                assert(bias_u.Shape(1) == q_with_bias_u.Shape(2));
                assert(bias_v.Shape(0) == bias_u.Shape(0));
                assert(bias_v.Shape(1) == bias_u.Shape(1));
                const float *q_data{query.DataImm()}, *u_bias_data{bias_u.DataImm()}, *v_bias_data{bias_v.DataImm()};
                float *qu_data{q_with_bias_u.Data()}, *qv_data{q_with_bias_v.Data()};
                for (uint32_t i = 0; i < q_with_bias_u.Shape(0); i++)
                {
                    for (uint32_t j = 0; j < bias_u.Numel(); j++)
                    {
                        *qu_data = *q_data + u_bias_data[j];
                        *qv_data = *q_data + v_bias_data[j];
                        qu_data++;
                        qv_data++;
                        q_data++;
                    }
                }
            }

            q_with_bias_u = tlib::ops::transpose(q_with_bias_u, 0, 1);
            q_with_bias_v = tlib::ops::transpose(q_with_bias_v, 0, 1);
        }

        // matrix_bd
        tlib::Tensor<float> matrix_bd;
        {
            // p
            auto p = tlib::ops::linear_deq(
                pos_emb,
                tlib::flash::LoadTensor<int8_t>(GetLayerTensorID(linear_pos_weight_base, layer_index), tlib::heap::Type::SRAM));
            p.View({p.Shape(0), kNumHeads, kHeadDimension});
            p = tlib::ops::transpose(p, 0, 1, tlib::heap::Type::SRAM);
            matrix_bd = RelativePositionalShift(tlib::ops::bmm(q_with_bias_v, p));
        }

        // matrix_ac
        tlib::Tensor<float> matrix_ac;
        {
            auto key = tlib::ops::linear_deq(
                x_q,
                tlib::flash::LoadTensor<int8_t>(GetLayerTensorID(linear_k_weight_base, layer_index), tlib::heap::Type::SRAM),
                tlib::flash::LoadTensor<int32_t>(GetLayerTensorID(linear_k_bias_base, layer_index), tlib::heap::Type::SRAM));
            key.View({key.Shape(0), kNumHeads, kHeadDimension});
            key = tlib::ops::transpose(key, 0, 1, tlib::heap::Type::SRAM);

            matrix_ac = tlib::ops::bmm(q_with_bias_u, key);
        }

        // matrix_bd = matrix_bd[..., :matrix_ac.shape[-1]]
        {
            auto shape = matrix_bd.Shape();
            shape.at(2) = matrix_ac.Shape(2);

            auto tmp = tlib::Tensor<float>(std::move(shape), false, 0);
            const uint32_t n_advance_bd{matrix_bd.Shape(2)};
            const uint32_t n_advance_tmp{tmp.Shape(2)};

            const float *bd_data{matrix_bd.DataImm()};
            float *tmp_data{tmp.Data()};

            for (uint32_t row = 0; row < matrix_bd.Shape(0) * matrix_bd.Shape(1); row++)
            {
                std::memcpy(tmp_data, bd_data, sizeof(float) * n_advance_tmp);
                tmp_data += n_advance_tmp;
                bd_data += n_advance_bd;
            }

            matrix_bd = tmp;
        }

        auto scores = tlib::ops::add_scale(matrix_ac, matrix_bd, kScale);

        scores = tlib::ops::softmax(scores);

        auto value = tlib::ops::linear_deq(
            x_q,
            tlib::flash::LoadTensor<int8_t>(GetLayerTensorID(linear_v_weight_base, layer_index), tlib::heap::Type::SRAM),
            tlib::flash::LoadTensor<int32_t>(GetLayerTensorID(linear_v_bias_base, layer_index), tlib::heap::Type::SRAM));
        value.View({value.Shape(0), kNumHeads, kHeadDimension});
        value = tlib::ops::transpose(value, 0, 1, tlib::heap::Type::SRAM);
        value = tlib::ops::transpose(value, 1, 2, tlib::heap::Type::SRAM);

        scores = tlib::ops::bmm(scores, value);

        scores = tlib::ops::transpose(scores, 0, 1);
        scores.View({scores.Shape(0), scores.Shape(1) * scores.Shape(2)});

        auto scores_q = tlib::ops::quantize(scores, linear_out_input_shift.at(layer_index));

        return tlib::ops::linear_deq(
            scores_q,
            tlib::flash::LoadTensor<int8_t>(GetLayerTensorID(linear_out_weight_base, layer_index), tlib::heap::Type::SRAM),
            tlib::flash::LoadTensor<int32_t>(GetLayerTensorID(linear_out_bias_base, layer_index), tlib::heap::Type::SRAM));
    }

} // conformer