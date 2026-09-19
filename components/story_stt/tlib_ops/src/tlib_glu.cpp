#include "tlib_glu.h"

#include <cmath>

#include <tlib_transpose.h>

namespace tlib::ops
{

    Tensor<float> glu(const Tensor<float> &x)
    {
        assert(x.Dim() == 2);
        assert(x.Shape(1) % 2 == 0);

        Tensor<float> out({x.Shape(1) / 2, x.Shape(0)}, false, 0);

        auto x_T = transpose(x, 0, 1);

        float *out_data = out.Data();
        const float *x_T_data = x_T.DataImm();
        const uint32_t half_x_numel = out.Numel();

        for (uint32_t i = 0; i < half_x_numel; i++)
        {
            out_data[i] = x_T_data[i] * (1.0f / (1.0f + expf(-x_T_data[i + half_x_numel])));
        }

        return out;
    }

} // tlib::ops