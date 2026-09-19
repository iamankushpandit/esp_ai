#include "tlib_softmax.h"

#include <math.h>

namespace tlib::ops
{

    Tensor<float> softmax(const TensorView<float> &x)
    {
        assert(x.Dim() == 3);

        auto y = Tensor<float>(x.Shape(), false, 0);

        const uint32_t n_rows{x.Shape(0) * x.Shape(1)};
        const uint32_t n_cols{x.Shape(1)};

        const float *x_data{x.DataImm()};
        float *y_data{y.Data()};

        for (uint32_t row = 0; row < n_rows; row++)
        {
            float row_max{x_data[0]};

            for (uint32_t col = 0; col < n_cols; col++)
            {
                if (x_data[col] > row_max)
                {
                    row_max = x_data[col];
                }
            }

            // sum(exp(x - max))
            float row_exp_sum{0.0f};
            for (uint32_t col = 0; col < n_cols; col++)
            {
                const float exp = expf(x_data[col] - row_max);
                y_data[col] = exp;
                row_exp_sum += exp;
            }

            const float inv_row_exp_sum{1.0f / row_exp_sum};

            for (uint32_t col = 0; col < n_cols; col++)
            {
                y_data[col] *= inv_row_exp_sum;
            }

            y_data += n_cols;
            x_data += n_cols;
        }

        return y;
    }

} // tlib::ops