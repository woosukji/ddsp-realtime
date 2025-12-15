#pragma once

#include "DDSPTypes.h"
#include <vector>
#include <optional>

namespace ddsp {

/**
 * Additive harmonic synthesizer
 *
 * Generates audio using phase-continuous additive synthesis based on
 * fundamental frequency and per-harmonic amplitude distributions.
 *
 * Features:
 * - 60 harmonics at model sample rate (16kHz)
 * - Midway interpolation (anti-swooping)
 * - Nyquist filtering (removes harmonics >= 8kHz)
 * - Harmonic normalization (sum to 1, scale by amplitude)
 *
 * Thread-safety: NOT thread-safe. Use from single thread only.
 */
class HarmonicSynthesizer {
public:
    /**
     * Constructor
     * @param num_harmonics Number of harmonics (default 60)
     * @param num_output_samples Samples per frame (default 320)
     * @param sample_rate Model sample rate (default 16000)
     */
    HarmonicSynthesizer(
        int num_harmonics = kHarmonicsSize,
        int num_output_samples = kModelHopSize,
        float sample_rate = kModelSampleRate_Hz
    );

    ~HarmonicSynthesizer() = default;

    /**
     * Render one frame of harmonic audio
     *
     * @param harmonic_distribution Harmonic amplitudes [num_harmonics]
     * @param amplitude Overall amplitude
     * @param f0_hz Fundamental frequency in Hz
     * @return Reference to internal buffer with synthesized audio [num_output_samples]
     */
    const std::vector<float>& render(
        std::vector<float>& harmonic_distribution,
        float amplitude,
        float f0_hz
    );

    /**
     * Reset internal state (phases, previous values)
     */
    void reset();

private:
    int num_harmonics_;
    int num_output_samples_;
    float sample_rate_;

    // Previous frame values for interpolation
    float previous_phase_;
    std::optional<float> previous_f0_;
    float previous_amplitude_;
    std::vector<float> previous_harmonic_distribution_;

    // Working buffers
    std::vector<float> harmonic_series_;            // [1, 2, 3, ..., num_harmonics]
    std::vector<float> frame_frequencies_;          // Frequencies for current frame
    std::vector<float> frequency_envelope_;         // Interpolated f0 [num_output_samples]
    std::vector<float> phases_;                     // Phase accumulator [num_output_samples]
    std::vector<std::vector<float>> harmonic_amplitudes_; // [num_harmonics][num_output_samples]
    std::vector<float> render_buffer_;              // Output buffer

    /**
     * Normalize harmonic distribution (sum to 1, scale by amplitude)
     * Also removes harmonics above Nyquist
     */
    void normalizeHarmonicDistribution(
        std::vector<float>& harmonic_distribution,
        float amplitude,
        float f0_hz
    );

    /**
     * Midway linear interpolation (anti-swooping)
     * First half: linear interpolation from first to last
     * Second half: constant at last value
     */
    void midwayLerp(float first, float last, std::vector<float>& result);

    /**
     * Standard linear interpolation between two values
     */
    void interpolateLinearly(
        std::vector<float>::iterator begin,
        std::vector<float>::iterator end,
        float first,
        float last
    );

    /**
     * Perform additive synthesis
     */
    const std::vector<float>& synthesizeHarmonics();
};

} // namespace ddsp
