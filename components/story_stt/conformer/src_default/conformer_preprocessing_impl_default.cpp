#include "conformer_preprocessing.h"

#include <config.h>

namespace conformer {

Preprocessor::Preprocessor() {
    scaled_hann_window_ = tlib::Tensor<float>({kNFFT}, false, 0);
    InitializeScaledHannWindow(scaled_hann_window_, kNFFT);
}

void Preprocessor::STFTStep(const float * x_data, float * y_data) {
    const float factor{(-2.0f * static_cast<float>(M_PI)) / static_cast<float>(kWinLength)};
    const uint32_t n_freq = kNFFT / 2;
    

    for (uint32_t omega = 0; omega < n_freq; omega++) {
        float real{0.0f}, imag{0.0f};
        for (uint32_t sample = 0; sample < kWinLength; sample++) {
            const float factor_curr{static_cast<float>(omega * sample) * factor};
            real += std::cosf(factor_curr) * x_data[sample];
            imag += std::sinf(factor_curr) * x_data[sample];
        }
        y_data[omega] = real*real + imag*imag;
    }
}

} // conformer