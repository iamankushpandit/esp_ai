#include "tlib_batchnorm.h"

#include <math.h>

namespace tlib::ops {

static constexpr const float kEpsilon{1e-05};

Tensor<float> batchnorm(const TensorView<float> & x, const TensorView<float> & w, const TensorView<float> & b, 
    const TensorView<float> & running_mean, const TensorView<float> & running_var) {

    assert(x.Dim() == 2);
    assert(w.Dim() == 1);
    assert(b.Dim() == 1);
    assert(running_mean.Dim() == 1);
    assert(running_var.Dim() == 1);

    const uint32_t n_rows{x.Shape(0)}, n_cols{x.Shape(1)};

    assert(w.Shape(0) == n_rows);
    assert(b.Shape(0) == n_rows);
    assert(running_mean.Shape(0) == n_rows);
    assert(running_var.Shape(0) == n_rows);

    Tensor<float> y(x.Shape(), false, 0);

    float * y_data{y.Data()};
    const float * x_data{x.DataImm()};
    const float * mean_data{running_mean.DataImm()};
    const float * var_data{running_var.DataImm()};
    const float * w_data{w.DataImm()};
    const float * b_data{b.DataImm()};
    for (uint32_t i = 0; i < n_rows; i++) {
        const float curr_mean = mean_data[i];
        const float curr_w = w_data[i];
        const float curr_b = b_data[i];
        const float denumerator = sqrtf(var_data[i] + kEpsilon);
        for (uint32_t j = 0; j < n_cols; j++) {
            *y_data = (*x_data - curr_mean) / denumerator * curr_w + curr_b;
            x_data++;
            y_data++;
        }
    }

    return y;
}

} // tlib::ops