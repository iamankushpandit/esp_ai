#include "tlib_layernorm.h"

#include <cmath>

namespace tlib::ops
{

    Tensor<float> layernorm(const TensorView<float> &x, const TensorView<float> &weights, const TensorView<float> &bias)
    {
        assert(x.Dim() == 2);
        uint32_t n_rows = x.Shape(0);
        uint32_t n_cols = x.Shape(1);

        assert(weights.Dim() == 1);
        uint32_t n_rows_weights = weights.Shape(0);

        assert(bias.Dim() == 1);
        uint32_t n_rows_bias = bias.Shape(0);

        assert(n_cols == n_rows_weights);
        assert(n_cols == n_rows_bias);

        Tensor<float> y({n_rows, n_cols}, false, 0);

        float mean, var;
        float *y_data{y.Data()};
        const float *x_data{x.DataImm()}, *w_data{weights.DataImm()}, *b_data{bias.DataImm()};
        for (uint32_t i = 0; i < n_rows; i++)
        {
            mean = 0.0f;
            for (uint32_t j = 0; j < n_cols; j++)
            {
                mean += x_data[j];
            }
            mean /= (float)n_cols;

            var = 0.0f;
            for (uint32_t j = 0; j < n_cols; j++)
            {
                const float tmp = x_data[j] - mean;
                var += tmp * tmp;
            }
            var /= (float)n_cols;

            const float scale = 1.0f / std::sqrtf(var + 1e-5f);

            for (uint32_t j = 0; j < n_cols; j++)
            {
                y_data[j] = (x_data[j] - mean) * scale * w_data[j] + b_data[j];
            }

            x_data += n_cols;
            y_data += n_cols;
        }

        return y;
    }

} // tlib::ops