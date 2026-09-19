#include "conformer_preprocessing.h"

#include <cmath>
#include <limits>

#include <config.h>

#include <tlib_flash.h>
#include <tlib_ops.h>

#include <conformer_tensor_ids.h>

namespace conformer
{

    void ApplyHannWindow(const int16_t *x, const float *scaled_hann_window, float *y, const uint16_t n_fft)
    {
        for (uint16_t i = 0; i < n_fft; i++)
        {
            y[i] = static_cast<float>(x[i]) * scaled_hann_window[i];
        }
    }

    tlib::Tensor<float> Preprocessor::Forward(const tlib::TensorView<int16_t> &x, const uint32_t valid_samples)
    {
        assert(x.Dim() == 1);
        assert(valid_samples <= x.Numel());

        const uint32_t n_frames = 1 + std::floor((valid_samples - kNFFT) / kHopLength);
        const uint32_t n_freq = kNFFT / 2;
        assert(n_frames > 0);

        tlib::Tensor<float> x_win({kWinLength}, false, 0, tlib::heap::Type::SRAM);
        const float *scaled_hann_window_data{scaled_hann_window_.DataImm()};
        const int16_t *x_data{x.DataImm()};
        float *x_win_data{x_win.Data()};

        tlib::Tensor<float> spec({n_frames, n_freq}, false, 0);
        float *spec_data{spec.Data()};

        // compute STFT
        for (uint32_t i = 0; i < n_frames; i++)
        {
            ApplyHannWindow(x_data + i * kHopLength, scaled_hann_window_data, x_win_data, kNFFT);
            STFTStep(x_win_data, spec_data + i * n_freq);
        }

        // apply mel filter banks
        {
            auto mel_filters = tlib::flash::LoadTensor<float>(TensorID::MEL_FILTER_FLOAT32, tlib::heap::Type::SRAM);
            spec = tlib::ops::linear(spec, mel_filters);
        }

        // log + normalization
        {
            const auto n_mel{spec.Shape(1)};
            auto mel_mean = tlib::flash::LoadTensor<float>(TensorID::MEL_MEAN_FLOAT32, tlib::heap::Type::SRAM);
            auto mel_std = tlib::flash::LoadTensor<float>(TensorID::MEL_STD_FLOAT32, tlib::heap::Type::SRAM);
            assert(mel_mean.Dim() == 1);
            assert(mel_std.Dim() == 1);
            assert(n_mel == mel_mean.Shape(0));
            assert(n_mel == mel_std.Shape(0));

            const float *mel_mean_data{mel_mean.DataImm()}, *mel_std_data{mel_std.DataImm()};
            float *mel_data{spec.Data()};

            // 2**-24
            static constexpr const float kZeroGuard{5.960464477539063e-08};

            for (uint32_t r = 0; r < n_frames; r++)
            {
                for (uint32_t c = 0; c < n_mel; c++)
                {
                    mel_data[c] = (std::logf(mel_data[c] + kZeroGuard) - mel_mean_data[c]) / mel_std_data[c];
                }
                mel_data += n_mel;
            }
        }

        return spec;
    }

} // conformer