#include "tlib_bmm.h"

#include <tlib_tensor.h>
#include <tlib_linear_impl.h>

namespace tlib::ops
{

    Tensor<float> bmm(const TensorView<float> &a, const TensorView<float> &b)
    {
        assert(a.Dim() == 3 && b.Dim() == 3);
        assert(a.Shape(0) == b.Shape(0));
        assert(a.Shape(2) == b.Shape(2));

        const uint32_t y_pages = a.Shape(0);
        const uint32_t y_rows = a.Shape(1);
        const uint32_t y_cols = b.Shape(1);
        const uint32_t n_inner = a.Shape(2);

        auto y = tlib::Tensor<float>({y_pages, y_rows, y_cols}, false, 0);

        float *y_data{y.Data()};
        const float *a_data{a.DataImm()}, *b_data{b.DataImm()};
        for (uint32_t p = 0; p < y_pages; p++)
        {
            impl::linear_impl(a_data, b_data, y_data, n_inner, y_rows, y_cols);
            a_data += a.Shape(1) * a.Shape(2);
            b_data += b.Shape(1) * b.Shape(2);
            y_data += y.Shape(1) * y.Shape(2);
        }

        return y;
    }

} // tlib::ops