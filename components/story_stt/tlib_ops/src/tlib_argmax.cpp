#include "tlib_argmax.h"

namespace tlib::ops
{

    Tensor<uint32_t> argmax(const TensorView<float> &x)
    {
        assert(x.Dim() == 2);

        const uint32_t n_rows = x.Shape(0);
        const uint32_t n_cols = x.Shape(1);

        auto y = Tensor<uint32_t>({n_rows}, false, 0);

        const float *x_data{x.DataImm()};
        uint32_t *y_data{y.Data()};

        float max_val;
        uint32_t max_idx;

        for (uint32_t row = 0; row < n_rows; row++)
        {
            max_val = *x_data;
            max_idx = 0;
            x_data++;
            for (uint32_t col = 1; col < n_cols; col++)
            {
                if (*x_data > max_val)
                {
                    max_val = *x_data;
                    max_idx = col;
                }
                x_data++;
            }
            *y_data = max_idx;
            y_data++;
        }

        return y;
    }

} // tlib::ops