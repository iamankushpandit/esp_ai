#include "tlib_linear_impl.h"

#include <tuple>

#include <tlib_linear_shared.h>

extern "C"
{

    void tlib_linear_b_relu_impl_lshift_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, int8_t *y, const uint32_t k16, const uint32_t n, const uint32_t m, const uint8_t lshift);
    void tlib_linear_b_relu_impl_rshift_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, int8_t *y, const uint32_t k16, const uint32_t n, const uint32_t m, const uint8_t rshift);

    void tlib_linear_relu_impl_lshift_esp32s3(const int8_t *a, const int8_t *b, int8_t *y, const uint32_t k16, const uint32_t n, const uint32_t m, const uint8_t lshift);
    void tlib_linear_relu_impl_rshift_esp32s3(const int8_t *a, const int8_t *b, int8_t *y, const uint32_t k16, const uint32_t n, const uint32_t m, const uint8_t rshift);

    void tlib_linear_deq_impl_shift00_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift01_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift02_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift03_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift04_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift05_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift06_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift07_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift08_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift09_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift10_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift11_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift12_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift13_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_deq_impl_shift14_esp32s3(const int8_t *a, const int8_t *b, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);

    void tlib_linear_b_deq_impl_shift00_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift01_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift02_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift03_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift04_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift05_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift06_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift07_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift08_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift09_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift10_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift11_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift12_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift13_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);
    void tlib_linear_b_deq_impl_shift14_esp32s3(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k16, const uint32_t n, const uint32_t m);

    void tlib_linear_impl_esp32s3(const float *a, const float *b, float *y, const uint32_t k4, const uint32_t n, const uint32_t m);
}

namespace tlib::ops::impl
{

    /**
     * Splits parameters of a linear operator such that it can be executed by two workers simultaneously.
     * Posts a shared execution request if shared execution is available and the calling task is the main
     * worker.
     *
     * @param   op_id   Operator ID
     * @param   n       Number of output rows
     * @param   m       Number of output cols
     * @param   k       Number of inner dimensions
     * @param   a       Pointer to left-side tensor data
     * @param   b       Pointer to right-side tensor data
     * @param   y       Pointer to output tensor data
     * @param   c       Pointer to bias data (optional)
     * @param   shift   Desired output quantization shift (optional)
     *
     * @returns tuple (n, y) with:
     *          n = number of rows for current worker
     *          y = output data pointer for current worker
     */
    template <typename dtype_in, typename dtype_out>
    std::tuple<uint32_t, const dtype_in *, dtype_out *> GetWorkerParameters(const shared::OperatorID op_id, const uint32_t n, const uint32_t m, const uint32_t k, const dtype_in *a, const dtype_in *b, dtype_out *y, const int32_t *c = nullptr, const uint8_t shift = 0)
    {
        bool request_transmitted{false};
        if (shared::IsSharedExecutionAvailable() && shared::IsMainWorker()) {
            request_transmitted = shared::Execute({
                .id = op_id,
                .a = a,
                .b = b,
                .c = c,
                .y = y,
                .k = k,
                .n = n,
                .m = m,
                .shift = shift
            });
        }

        const uint32_t n1 = (request_transmitted || shared::IsCoWorker()) ? n / 2 : n;
        const uint32_t n2{n - n1};
        const dtype_in *a2{a + n1 * k};
        dtype_out *y2{y + n1 * m};
        if (shared::IsMainWorker())
        {
            
            return {n1, a, y};
        }
        else
        {
            return {n2, a2, y2};
        }
    }

    void linear_b_relu_impl_lshift(const int8_t *a, const int8_t *b, const int32_t *c, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t lshift)
    {
        assert(k % 16 == 0);
        const uint32_t k16 = k / 16;
        auto [n_curr, a_curr, y_curr] = GetWorkerParameters(shared::OperatorID::LINEAR_B_RELU_LSHIFT, n, m, k, a, b, y, c, lshift);
        tlib_linear_b_relu_impl_lshift_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m, lshift);
        shared::Synchronize();
    }

    void linear_b_relu_impl_rshift(const int8_t *a, const int8_t *b, const int32_t *c, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t rshift)
    {
        assert(k % 16 == 0);
        const uint32_t k16 = k / 16;
        auto [n_curr, a_curr, y_curr] = GetWorkerParameters(shared::OperatorID::LINEAR_B_RELU_RSHIFT, n, m, k, a, b, y, c, rshift);
        tlib_linear_b_relu_impl_rshift_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m, rshift);
        shared::Synchronize();
    }

    void linear_relu_impl_lshift(const int8_t *a, const int8_t *b, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t lshift)
    {
        assert(k % 16 == 0);
        const uint32_t k16 = k / 16;
        auto [n_curr, a_curr, y_curr] = GetWorkerParameters(shared::OperatorID::LINEAR_RELU_LSHIFT, n, m, k, a, b, y, nullptr, lshift);
        tlib_linear_relu_impl_lshift_esp32s3(a_curr, b, y_curr, k16, n_curr, m, lshift);
        shared::Synchronize();
    }

    void linear_relu_impl_rshift(const int8_t *a, const int8_t *b, int8_t *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t rshift)
    {
        assert(k % 16 == 0);
        const uint32_t k16 = k / 16;
        auto [n_curr, a_curr, y_curr] = GetWorkerParameters(shared::OperatorID::LINEAR_RELU_RSHIFT, n, m, k, a, b, y, nullptr, rshift);
        tlib_linear_relu_impl_rshift_esp32s3(a_curr, b, y_curr, k16, n_curr, m, rshift);
        shared::Synchronize();
    }

    void linear_deq_impl(const int8_t *a, const int8_t *b, float *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t shift)
    {
        assert(shift < 15);
        assert(k % 16 == 0);
        const uint32_t k16 = k / 16;
        auto [n_curr, a_curr, y_curr] = GetWorkerParameters(shared::OperatorID::LINEAR_DEQ, n, m, k, a, b, y, nullptr, shift);

        switch (shift)
        {
        case 0:
            tlib_linear_deq_impl_shift00_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 1:
            tlib_linear_deq_impl_shift01_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 2:
            tlib_linear_deq_impl_shift02_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 3:
            tlib_linear_deq_impl_shift03_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 4:
            tlib_linear_deq_impl_shift04_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 5:
            tlib_linear_deq_impl_shift05_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 6:
            tlib_linear_deq_impl_shift06_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 7:
            tlib_linear_deq_impl_shift07_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 8:
            tlib_linear_deq_impl_shift08_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 9:
            tlib_linear_deq_impl_shift09_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 10:
            tlib_linear_deq_impl_shift10_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 11:
            tlib_linear_deq_impl_shift11_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 12:
            tlib_linear_deq_impl_shift12_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 13:
            tlib_linear_deq_impl_shift13_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        case 14:
            tlib_linear_deq_impl_shift14_esp32s3(a_curr, b, y_curr, k16, n_curr, m);
            return;
        default:
            assert(0);
            break;
        }
        shared::Synchronize();
    }

    void linear_b_deq_impl(const int8_t *a, const int8_t *b, const int32_t *c, float *y, const uint32_t k, const uint32_t n, const uint32_t m, const uint8_t shift)
    {
        assert(shift < 15);
        assert(k % 16 == 0);
        const uint32_t k16 = k / 16;
        auto [n_curr, a_curr, y_curr] = GetWorkerParameters(shared::OperatorID::LINEAR_B_DEQ, n, m, k, a, b, y, c, shift);
        switch (shift)
        {
        case 0:
            tlib_linear_b_deq_impl_shift00_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 1:
            tlib_linear_b_deq_impl_shift01_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 2:
            tlib_linear_b_deq_impl_shift02_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 3:
            tlib_linear_b_deq_impl_shift03_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 4:
            tlib_linear_b_deq_impl_shift04_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 5:
            tlib_linear_b_deq_impl_shift05_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 6:
            tlib_linear_b_deq_impl_shift06_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 7:
            tlib_linear_b_deq_impl_shift07_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 8:
            tlib_linear_b_deq_impl_shift08_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 9:
            tlib_linear_b_deq_impl_shift09_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 10:
            tlib_linear_b_deq_impl_shift10_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 11:
            tlib_linear_b_deq_impl_shift11_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 12:
            tlib_linear_b_deq_impl_shift12_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 13:
            tlib_linear_b_deq_impl_shift13_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        case 14:
            tlib_linear_b_deq_impl_shift14_esp32s3(a_curr, b, c, y_curr, k16, n_curr, m);
            return;
        default:
            assert(0);
            break;
        }
        shared::Synchronize();
    }

    void linear_impl(const float *a, const float *b, float *y, const uint32_t k, const uint32_t n, const uint32_t m)
    {
        assert(k % 4 == 0);
        const uint32_t k4 = k / 4;
        auto [n_curr, a_curr, y_curr] = GetWorkerParameters(shared::OperatorID::LINEAR, n, m, k, a, b, y, nullptr, 0);
        tlib_linear_impl_esp32s3(a_curr, b, y_curr, k4, n_curr, m);
        shared::Synchronize();
    }

} // tlib::ops::impl