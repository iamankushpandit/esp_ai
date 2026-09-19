#ifndef CONFORMER_PREPROCESSING_H_
#define CONFORMER_PREPROCESSING_H_

#include <cmath>
#include <cassert>
#include <cstdint>

#include <limits>

#include <tlib_tensor.h>

namespace conformer
{

    /**
     * Preprocessor for the conformer. Calculates the normalized log-mel spectrogram from an input audio.
     */
    class Preprocessor
    {
    public:
        /**
         * Initializes the preprocessor and creates the necessary resources.
         * 
         * Important:   The preprocessor internally allocates some tensors in the PSRAM heap,
         *              which means that the heap must be initialized already.
         */
        explicit Preprocessor(void);

        /**
         * Delete move and copy constructors/assignments
         */
        Preprocessor(const Preprocessor &) = delete;
        Preprocessor(const Preprocessor &&) = delete;
        Preprocessor &operator=(const Preprocessor &) = delete;
        Preprocessor &&operator=(const Preprocessor &&) = delete;

        /**
         * Performs a short-time fourier transform (STFT) and returns the squared
         * magnitude of the complex-valued result.
         *
         * @param   x               [n] int16_t tensor, input audio signal
         * @param   valid_samples   The number of valid samples in the input audio signal
         *
         * @returns [m, kNFFT/2] float tensor, containing the squared STFT magnitudes
         */
        tlib::Tensor<float> Forward(const tlib::TensorView<int16_t> &x, const uint32_t valid_samples);

    private:
        /**
         * Creates a Hann-window which is scaled such that it implicitly performs audio normalization
         * from int16_t [-32,768, 32,767] to float [-1.0, 1.0] when applied.
         * 
         * @param   window  [n_fft] float tensor, buffer for scaled hann window
         * @param   n_fft   Size of fourier transform
         */
        inline constexpr void InitializeScaledHannWindow(tlib::Tensor<float> & window, const uint16_t n_fft)
        {
            assert(window.Numel() == n_fft && "Window size differs from FFT size.");
            float * win_data{window.Data()};
            for (uint32_t i = 0; i < n_fft; i++)
            {
                const float tmp = std::sinf((M_PI * i) / static_cast<float>(n_fft - 1));
                win_data[i] = tmp * tmp / static_cast<float>(std::numeric_limits<int16_t>::max());
            }
        }

        /**
         * Performs a single step of the STFT by calculating the FFT over a window
         * of size kNFFT.
         * 
         * @param   x_data  Input buffer containing windowed data. Must be of size kWinLength.
         * @param   y_data  Output buffer for STFT result. Must be of size kNFFT/2.
         */
        void STFTStep(const float *x_data, float *y_data);

        tlib::Tensor<float> scaled_hann_window_; /*!< Buffer for scaled Hann window */
        tlib::Tensor<float> fft_coefficients_;   /*!< Buffer for FFT coefficients */
        tlib::Tensor<float> fft_buffer_;         /*!< Buffer for FFT intermediate results */
    };

} // conformer

#endif