#include "tlib_linear.h"

#include <cmath>
#include <numeric>
#include <algorithm>

#include <tlib_linear_impl.h>

namespace tlib::ops
{

    template <typename dtype>
    constexpr bool ValidateDimensions(const TensorView<dtype> &x, const TensorView<dtype> &w)
    {
        if (x.Dim() != 2 || w.Dim() != 2)
            return false;
        if (x.Shape(1) != w.Shape(1))
            return false;
        return true;
    }

    template <typename dtype1, typename dtype2>
    constexpr bool ValidateDimensions(const TensorView<dtype1> &x, const TensorView<dtype1> &w, const TensorView<dtype2> &b)
    {
        if (!ValidateDimensions(x, w))
            return false;
        if (b.Dim() != 1 || b.Shape(0) != w.Shape(0))
            return false;
        return true;
    }

    Tensor<int8_t> linear_relu(const TensorView<int8_t> &x, const TensorView<int8_t> &w, const TensorView<int32_t> &b, const uint8_t y_shift)
    {
        assert(ValidateDimensions(x, w, b));

        assert(x.Quantized());
        assert(w.Quantized());
        assert(b.Quantized());

        const auto y_rows = x.Shape(0);
        const auto y_cols = w.Shape(0);
        const auto n_inner = x.Shape(1);

        Tensor<int8_t> y({y_rows, y_cols}, true, y_shift);

        const int8_t shift_diff = (int8_t)y_shift - (int8_t)b.Shift();

        if (shift_diff < 0)
        {
            impl::linear_b_relu_impl_rshift(x.DataImm(), w.DataImm(), b.DataImm(), y.Data(), n_inner, y_rows, y_cols, -shift_diff);
        }
        else
        {
            impl::linear_b_relu_impl_lshift(x.DataImm(), w.DataImm(), b.DataImm(), y.Data(), n_inner, y_rows, y_cols, shift_diff);
        }

        return y;
    }

    Tensor<int8_t> linear_relu(const TensorView<int8_t> &x, const TensorView<int8_t> &w, const uint8_t y_shift)
    {
        assert(ValidateDimensions(x, w));

        assert(x.Quantized());
        assert(w.Quantized());

        const auto y_rows = x.Shape(0);
        const auto y_cols = w.Shape(0);
        const auto n_inner = x.Shape(1);

        Tensor<int8_t> y({y_rows, y_cols}, true, y_shift);

        const int8_t shift_diff = (int8_t)y_shift - (int8_t)(x.Shift() + w.Shift());

        if (shift_diff < 0)
        {
            impl::linear_relu_impl_rshift(x.DataImm(), w.DataImm(), y.Data(), n_inner, y_rows, y_cols, -shift_diff);
        }
        else
        {
            impl::linear_relu_impl_lshift(x.DataImm(), w.DataImm(), y.Data(), n_inner, y_rows, y_cols, shift_diff);
        }

        return y;
    }

    Tensor<float> linear_deq(const TensorView<int8_t> &x, const TensorView<int8_t> &w, const TensorView<int32_t> &b)
    {
        assert(ValidateDimensions(x, w, b));

        assert(x.Quantized());
        assert(w.Quantized());
        assert(b.Quantized());

        const auto y_rows = x.Shape(0);
        const auto y_cols = w.Shape(0);
        const auto n_inner = x.Shape(1);

        Tensor<float> y({y_rows, y_cols}, false, 0);

        const uint8_t shift = b.Shift();

        impl::linear_b_deq_impl(x.DataImm(), w.DataImm(), b.DataImm(), y.Data(), n_inner, y_rows, y_cols, shift);

        return y;
    }

    Tensor<float> linear_deq(const TensorView<int8_t> &x, const TensorView<int8_t> &w)
    {
        assert(ValidateDimensions(x, w));

        assert(x.Quantized());
        assert(w.Quantized());

        const auto y_rows = x.Shape(0);
        const auto y_cols = w.Shape(0);
        const auto n_inner = x.Shape(1);

        Tensor<float> y({y_rows, y_cols}, false, 0);

        const uint8_t shift = x.Shift() + w.Shift();

        impl::linear_deq_impl(x.DataImm(), w.DataImm(), y.Data(), n_inner, y_rows, y_cols, shift);

        return y;
    }

    Tensor<float> linear_relu_deq(const TensorView<int8_t> &x, const TensorView<int8_t> &w, const TensorView<int32_t> &b)
    {
        assert(ValidateDimensions(x, w, b));

        assert(x.Quantized());
        assert(w.Quantized());
        assert(b.Quantized());

        const auto y_rows = x.Shape(0);
        const auto y_cols = w.Shape(0);
        const auto n_inner = x.Shape(1);

        Tensor<float> y({y_rows, y_cols}, false, 0);

        const uint8_t shift = b.Shift();

        impl::linear_b_relu_deq_impl(x.DataImm(), w.DataImm(), b.DataImm(), y.Data(), n_inner, y_rows, y_cols, shift);

        return y;
    }

    Tensor<float> linear(const TensorView<float> &x, const TensorView<float> &w)
    {
        assert(ValidateDimensions(x, w));

        const auto y_rows = x.Shape(0);
        const auto y_cols = w.Shape(0);
        const auto n_inner = x.Shape(1);

        Tensor<float> y({y_rows, y_cols}, false, 0);

        impl::linear_impl(x.DataImm(), w.DataImm(), y.Data(), n_inner, y_rows, y_cols);

        return y;
    }

} // tlib::ops