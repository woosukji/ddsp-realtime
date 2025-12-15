#include "NoiseSynthesizer.h"
#include <algorithm>
#include <cmath>

namespace ddsp {

namespace {
    constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
}

NoiseSynthesizer::NoiseSynthesizer(int num_noise_amps, int num_output_samples)
    : num_noise_amps_(num_noise_amps)
    , num_output_samples_(num_output_samples)
    , impulse_response_size_((num_noise_amps - 1) * 2)  // 128 for 65 bands
    , window_fft_(7)    // 2^7 = 128 point FFT
    , convolve_fft_(9)  // 2^9 = 512 point FFT
    , rng_(std::random_device{}())
    , noise_dist_(-1.0f, 1.0f)
{
    // Allocate buffers
    magnitudes_complex_.resize(window_fft_.getSize());
    zp_hann_window_.resize(impulse_response_size_);
    windowed_impulse_response_.resize(convolve_fft_.getSize() * 2);  // 1024
    white_noise_.resize(convolve_fft_.getSize() * 2);                 // 1024
    noise_audio_.resize(num_output_samples_);

    // Create zero-phase Hann window
    createZeroPhaseHannWindow();
}

void NoiseSynthesizer::reset() {
    std::fill(noise_audio_.begin(), noise_audio_.end(), 0.0f);
    std::fill(windowed_impulse_response_.begin(), windowed_impulse_response_.end(), 0.0f);
    std::fill(white_noise_.begin(), white_noise_.end(), 0.0f);
}

const std::vector<float>& NoiseSynthesizer::render(const std::vector<float>& magnitudes) {
    // Convert frequency magnitudes to time-domain FIR filter
    applyWindowToImpulseResponse(magnitudes);

    // Filter white noise using frequency-domain convolution
    convolve();

    return noise_audio_;
}

void NoiseSynthesizer::createZeroPhaseHannWindow() {
    // Create standard Hann window
    for (int i = 0; i < impulse_response_size_; ++i) {
        zp_hann_window_[i] = 0.5f * (1.0f - std::cos(kTwoPi * i / static_cast<float>(impulse_response_size_)));
    }

    // Rotate to zero-phase form (center at index 0)
    std::rotate(
        zp_hann_window_.begin(),
        zp_hann_window_.begin() + impulse_response_size_ / 2,
        zp_hann_window_.end()
    );
}

void NoiseSynthesizer::applyWindowToImpulseResponse(const std::vector<float>& mags) {
    // Clear complex buffer and set real parts to magnitudes
    std::fill(magnitudes_complex_.begin(), magnitudes_complex_.end(), std::complex<float>(0.0f, 0.0f));

    for (int i = 0; i < static_cast<int>(mags.size()) && i < static_cast<int>(magnitudes_complex_.size()); ++i) {
        magnitudes_complex_[i] = std::complex<float>(mags[i], 0.0f);
    }

    // Cast complex buffer to float for JUCE FFT
    // JUCE FFT expects interleaved [real, imag, real, imag, ...]
    auto* impulse_response = reinterpret_cast<float*>(magnitudes_complex_.data());

    // Perform IFFT to get time-domain impulse response
    window_fft_.performRealOnlyInverseTransform(impulse_response);

    // Apply zero-phase Hann window
    for (int i = 0; i < impulse_response_size_; ++i) {
        impulse_response[i] *= zp_hann_window_[i];
    }

    // Rotate to causal form
    std::rotate(
        impulse_response,
        impulse_response + window_fft_.getSize() / 2,
        impulse_response + window_fft_.getSize()
    );

    // Zero-pad for convolution (copy to larger buffer)
    std::fill(windowed_impulse_response_.begin(), windowed_impulse_response_.end(), 0.0f);
    std::copy(impulse_response, impulse_response + impulse_response_size_, windowed_impulse_response_.begin());
}

void NoiseSynthesizer::convolve() {
    // Generate white noise [-1, 1]
    for (auto& sample : white_noise_) {
        sample = noise_dist_(rng_);
    }

    // FFT both signals
    convolve_fft_.performRealOnlyForwardTransform(white_noise_.data());
    convolve_fft_.performRealOnlyForwardTransform(windowed_impulse_response_.data());

    // Cast to complex for multiplication
    auto* white_noise_freqs = reinterpret_cast<std::complex<float>*>(white_noise_.data());
    auto* ir_freqs = reinterpret_cast<std::complex<float>*>(windowed_impulse_response_.data());

    // Multiply in frequency domain (convolution)
    int num_bins = convolve_fft_.getSize() / 2 + 1;
    for (int i = 0; i < num_bins; ++i) {
        white_noise_freqs[i] *= ir_freqs[i];
    }

    // IFFT back to time domain
    convolve_fft_.performRealOnlyInverseTransform(white_noise_.data());

    // Crop and compensate for filter group delay
    cropAndCompensateDelay(white_noise_, impulse_response_size_);
}

void NoiseSynthesizer::cropAndCompensateDelay(const std::vector<float>& input_audio, int ir_size) {
    // Group delay for linear phase FIR = (ir_size - 1) / 2
    int delay = (ir_size - 1) / 2 - 1;

    // Copy samples from delayed position
    for (int i = 0; i < num_output_samples_; ++i) {
        int src_idx = delay + i;
        if (src_idx >= 0 && src_idx < static_cast<int>(input_audio.size())) {
            noise_audio_[i] = input_audio[src_idx];
        } else {
            noise_audio_[i] = 0.0f;
        }
    }
}

} // namespace ddsp
