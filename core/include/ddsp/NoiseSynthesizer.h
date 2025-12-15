#pragma once

#include "DDSPTypes.h"
#include <vector>
#include <random>
#include <complex>
#include <juce_dsp/juce_dsp.h>

namespace ddsp {

/**
 * Filtered noise synthesizer using FFT-based convolution
 *
 * Generates noise filtered by a time-varying FIR filter designed
 * from frequency magnitudes using the frequency-sampling method.
 *
 * Features:
 * - Frequency-sampling FIR design (IFFT of magnitude spectrum)
 * - Zero-phase Hann window (128 taps)
 * - Frequency-domain convolution with white noise
 * - Group delay compensation
 *
 * Thread-safety: NOT thread-safe. Use from single thread only.
 */
class NoiseSynthesizer {
public:
    /**
     * Constructor
     * @param num_noise_amps Number of noise bands (default 65)
     * @param num_output_samples Samples per frame (default 320)
     */
    NoiseSynthesizer(
        int num_noise_amps = kNoiseAmpsSize,
        int num_output_samples = kModelHopSize
    );

    ~NoiseSynthesizer() = default;

    /**
     * Render one frame of filtered noise
     *
     * @param magnitudes Filter magnitudes for each frequency band [num_noise_amps]
     * @return Reference to internal buffer with synthesized noise [num_output_samples]
     */
    const std::vector<float>& render(const std::vector<float>& magnitudes);

    /**
     * Reset internal state
     */
    void reset();

private:
    int num_noise_amps_;
    int num_output_samples_;
    int impulse_response_size_;  // (num_noise_amps - 1) * 2 = 128

    // FFT objects (JUCE)
    juce::dsp::FFT window_fft_;   // 128-point for windowing
    juce::dsp::FFT convolve_fft_; // 512-point for convolution

    // Random number generation
    std::mt19937 rng_;
    std::uniform_real_distribution<float> noise_dist_;

    // Buffers
    std::vector<std::complex<float>> magnitudes_complex_; // For IFFT
    std::vector<float> zp_hann_window_;                   // Zero-phase Hann window
    std::vector<float> windowed_impulse_response_;        // Windowed IR for convolution
    std::vector<float> white_noise_;                      // White noise buffer
    std::vector<float> noise_audio_;                      // Output buffer

    /**
     * Create zero-phase Hann window
     */
    void createZeroPhaseHannWindow();

    /**
     * Apply window to impulse response
     * Converts frequency magnitudes to time-domain FIR filter
     */
    void applyWindowToImpulseResponse(const std::vector<float>& mags);

    /**
     * Perform frequency-domain convolution
     */
    void convolve();

    /**
     * Crop output and compensate for filter group delay
     */
    void cropAndCompensateDelay(const std::vector<float>& input_audio, int ir_size);
};

} // namespace ddsp
