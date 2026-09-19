#include "tlib_relu.h"

namespace tlib::ops
{

    Tensor<float> relu(const TensorView<float> &x)
    {
        Tensor<float> y(x.Shape(), false, 0);

        const float *x_data{x.DataImm()};
        float *y_data{y.Data()};

        for (uint32_t i = 0; i < x.Numel(); i++)
        {
            y_data[i] = std::max<float>(x_data[i], 0.0f);
        }

        return y;
    }

} // tlib::ops