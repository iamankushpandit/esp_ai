#include <tlib_quantize.h>

#include <cmath>

#include <tlib_quantize_impl.h>

namespace tlib::ops
{

    Tensor<int8_t> quantize(const TensorView<float> &x, const uint8_t shift, const heap::Type location)
    {
        assert(shift < 8);
        assert(!x.Quantized());
        auto y = Tensor<int8_t>(x.Shape(), true, shift, location);

        impl::quantize_impl(x.DataImm(), y.Data(), x.Numel(), shift);

        return y;
    }

    Tensor<float> dequantize(const TensorView<int8_t> &x)
    {
        assert(x.Quantized());

        const float scale = 1.0f / std::powf(2.0f, x.Shift());

        auto y = Tensor<float>(x.Shape(), false, 0);
        const int8_t *x_data{x.DataImm()};
        float *y_data{y.Data()};

        for (uint32_t i = 0; i < x.Numel(); i++)
        {
            y_data[i] = ((float)x_data[i]) * scale;
        }

        return y;
    }

} // tlib::ops