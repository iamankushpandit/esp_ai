#include "conformer_pos_encode.h"

#include <conformer_tensor_ids.h>
#include <tlib_flash.h>

namespace conformer
{

    static constexpr const float kScale{13.2664991614216f}; /*!< sqrt(kEmbeddingDimension) */

    std::tuple<tlib::Tensor<float>, tlib::Tensor<int8_t>> PosEncode(const tlib::TensorView<float> &x)
    {
        auto pos_enc = tlib::flash::MapTensor<int8_t>(POS_ENC_Q_INT8);
        const uint32_t enc_rows{pos_enc.Shape(0)};
        const uint32_t enc_cols{pos_enc.Shape(1)};

        const uint32_t in_len = x.Shape(0);
        assert(enc_rows / 2 > in_len);
        assert(enc_cols == x.Shape(1));

        const uint32_t center_pos{enc_rows / 2 + 1};
        const uint32_t start_pos{center_pos - in_len};
        const uint32_t end_pos{center_pos + in_len - 1};

        auto x_pos_enc = tlib::Tensor<int8_t>({end_pos - start_pos, enc_cols}, pos_enc.Quantized(), pos_enc.Shift());
        const int8_t *pe_data{pos_enc.DataImm() + start_pos * enc_cols};
        int8_t *x_pe_data{x_pos_enc.Data()};
        std::memcpy(x_pe_data, pe_data, x_pos_enc.Numel());

        auto x_scaled = tlib::Tensor<float>(x.Shape(), false, 0);
        const float *x_data{x.DataImm()};
        float *x_scaled_data{x_scaled.Data()};
        for (uint32_t i = 0; i < x_scaled.Numel(); i++)
        {
            x_scaled_data[i] = x_data[i] * kScale;
        }

        return {x_scaled, x_pos_enc};
    }

} // conformer