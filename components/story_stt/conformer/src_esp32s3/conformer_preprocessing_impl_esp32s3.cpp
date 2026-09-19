#include "conformer_preprocessing.h"

#include <esp_dsp.h>

#include <config.h>

namespace conformer {

Preprocessor::Preprocessor(void) {
    fft_buffer_ = tlib::Tensor<float>({kNFFT}, false, 0);
    fft_coefficients_ = tlib::Tensor<float>({static_cast<uint32_t>(kNFFT * 4)}, false, 0);
    scaled_hann_window_ = tlib::Tensor<float>({kNFFT}, false, 0);
    InitializeScaledHannWindow(scaled_hann_window_, kNFFT);
    const esp_err_t err = dsps_fft4r_init_fc32(fft_coefficients_.Data(), kNFFT >> 1);
    assert(err == ESP_OK);
}

void Preprocessor::STFTStep(const float * x_data, float * y_data) {
    const uint16_t n_freq{static_cast<uint16_t>(kNFFT / 2)};
    float * fft_buffer_data{fft_buffer_.Data()};
    std::memcpy(fft_buffer_data, x_data, sizeof(float) * kNFFT);
    // Perform radix 4 fft of size kNFFT
    dsps_fft4r_fc32(fft_buffer_data, n_freq);
    dsps_bit_rev4r_fc32(fft_buffer_data, n_freq);
    dsps_cplx2real_fc32(fft_buffer_data, n_freq);

    for(uint16_t i = 0; i < n_freq; i++) {
        y_data[i] = fft_buffer_data[2*i] * fft_buffer_data[2*i] + fft_buffer_data[2*i + 1] * fft_buffer_data[2*i + 1];
    }
}

} // conformer