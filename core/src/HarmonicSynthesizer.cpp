#include "HarmonicSynthesizer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace ddsp {

namespace {
    constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
}

HarmonicSynthesizer::HarmonicSynthesizer(int num_harmonics, int num_output_samples, float sample_rate)
    : num_harmonics_(num_harmonics)
    , num_output_samples_(num_output_samples)
    , sample_rate_(sample_rate)
    , previous_phase_(0.0f)
    , previous_amplitude_(0.0f)
{
    // Create harmonic series [1, 2, 3, ..., num_harmonics]
    harmonic_series_.resize(num_harmonics_);
    std::iota(harmonic_series_.begin(), harmonic_series_.end(), 1.0f);

    // Allocate working buffers
    previous_harmonic_distribution_.resize(num_harmonics_, 0.0f);
    frame_frequencies_.resize(num_harmonics_);
    frequency_envelope_.resize(num_output_samples_);
    phases_.resize(num_output_samples_);
    render_buffer_.resize(num_output_samples_);

    // Allocate 2D harmonic amplitudes [num_harmonics][num_output_samples]
    harmonic_amplitudes_.resize(num_harmonics_);
    for (int i = 0; i < num_harmonics_; ++i) {
        harmonic_amplitudes_[i].resize(num_output_samples_, 0.0f);
    }
}

void HarmonicSynthesizer::reset() {
    previous_phase_ = 0.0f;
    previous_f0_.reset();
    previous_amplitude_ = 0.0f;
    std::fill(previous_harmonic_distribution_.begin(), previous_harmonic_distribution_.end(), 0.0f);
    std::fill(render_buffer_.begin(), render_buffer_.end(), 0.0f);
}

const std::vector<float>& HarmonicSynthesizer::render(
    std::vector<float>& harmonic_distribution,
    float amplitude,
    float f0_hz)
{
    // Normalize distribution and apply amplitude
    normalizeHarmonicDistribution(harmonic_distribution, amplitude, f0_hz);
    previous_amplitude_ = amplitude;

    // Interpolate frequency envelope using midway lerp
    float prev_f0 = previous_f0_.value_or(f0_hz);
    midwayLerp(prev_f0, f0_hz, frequency_envelope_);
    previous_f0_ = f0_hz;

    // Interpolate each harmonic's amplitude
    for (int i = 0; i < num_harmonics_; ++i) {
        midwayLerp(previous_harmonic_distribution_[i], harmonic_distribution[i], harmonic_amplitudes_[i]);
    }
    previous_harmonic_distribution_ = harmonic_distribution;

    return synthesizeHarmonics();
}

void HarmonicSynthesizer::normalizeHarmonicDistribution(
    std::vector<float>& harmonic_distribution,
    float amplitude,
    float f0_hz)
{
    // Calculate frequencies for each harmonic: f0 * [1, 2, 3, ..., num_harmonics]
    for (int i = 0; i < num_harmonics_; ++i) {
        frame_frequencies_[i] = harmonic_series_[i] * f0_hz;
    }

    // Remove harmonics above Nyquist (sample_rate / 2)
    float nyquist = sample_rate_ / 2.0f;
    for (int i = 0; i < num_harmonics_; ++i) {
        if (frame_frequencies_[i] >= nyquist) {
            harmonic_distribution[i] = 0.0f;
        }
    }

    // Normalize so coefficients sum to 1
    float total = std::accumulate(harmonic_distribution.begin(), harmonic_distribution.end(), 0.0f);
    if (total != 0.0f) {
        float scale = 1.0f / total;
        for (int i = 0; i < num_harmonics_; ++i) {
            harmonic_distribution[i] *= scale;
        }
    }

    // Scale by amplitude
    for (int i = 0; i < num_harmonics_; ++i) {
        harmonic_distribution[i] *= amplitude;
    }
}

void HarmonicSynthesizer::midwayLerp(float first, float last, std::vector<float>& result) {
    // First half: linear interpolation from first to last
    // Second half: constant at last value
    // This prevents "swooping" artifacts over the 20ms hop
    auto middle = result.begin() + result.size() / 2;
    interpolateLinearly(result.begin(), middle, first, last);
    std::fill(middle, result.end(), last);
}

void HarmonicSynthesizer::interpolateLinearly(
    std::vector<float>::iterator begin,
    std::vector<float>::iterator end,
    float first,
    float last)
{
    size_t num_samples = std::distance(begin, end);
    if (num_samples == 0) return;

    for (size_t i = 0; i < num_samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(num_samples);
        *(begin + i) = first + t * (last - first);
    }
}

const std::vector<float>& HarmonicSynthesizer::synthesizeHarmonics() {
    // Convert Hz to radians per sample
    for (int i = 0; i < num_output_samples_; ++i) {
        frequency_envelope_[i] *= kTwoPi / sample_rate_;
    }

    // Cumulative sum = instantaneous phase (integration)
    std::partial_sum(frequency_envelope_.begin(), frequency_envelope_.end(), phases_.begin());

    // Add previous total phase for continuity
    for (int i = 0; i < num_output_samples_; ++i) {
        phases_[i] += previous_phase_;
    }

    // Wrap and store phase for next frame
    previous_phase_ = std::fmod(phases_.back(), kTwoPi);

    // Clear output buffer
    std::fill(render_buffer_.begin(), render_buffer_.end(), 0.0f);

    // Generate sinusoids for each harmonic and accumulate
    for (int h = 0; h < num_harmonics_; ++h) {
        int harmonic_order = h + 1;  // 1, 2, 3, ...

        for (int s = 0; s < num_output_samples_; ++s) {
            float phase = phases_[s] * harmonic_order;
            float sinusoid = std::sin(phase) * harmonic_amplitudes_[h][s];
            render_buffer_[s] += sinusoid;
        }
    }

    return render_buffer_;
}

} // namespace ddsp
