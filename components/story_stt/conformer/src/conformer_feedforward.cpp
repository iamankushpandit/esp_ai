#include "conformer_feedforward.h"

#include <tlib_quantize.h>
#include <tlib_linear.h>
#include <tlib_flash.h>
#include <tlib_heap.h>

namespace conformer
{

    tlib::Tensor<float> FeedForward(const tlib::TensorView<float> &x,
                                    const TensorID weight0,
                                    const TensorID weight1,
                                    const TensorID bias0,
                                    const TensorID bias1,
                                    const uint8_t input0_shift,
                                    const uint8_t input1_shift)
    {
        auto x_q = tlib::ops::quantize(x, input0_shift);

        // linear 0 + relu
        {
            auto w = tlib::flash::LoadTensor<int8_t>(weight0, tlib::heap::Type::SRAM);
            auto b = tlib::flash::LoadTensor<int32_t>(bias0, tlib::heap::Type::SRAM);
            x_q = tlib::ops::linear_relu(x_q, w, b, input1_shift);
        }

        // linear 1 + dequantize
        auto w = tlib::flash::LoadTensor<int8_t>(weight1, tlib::heap::Type::SRAM);
        auto b = tlib::flash::LoadTensor<int32_t>(bias1, tlib::heap::Type::SRAM);
        return tlib::ops::linear_deq(x_q, w, b);
    }

} // conformer