#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <string_view>

namespace ddsp {

// ============================================================================
// Model Constants (from ddsp-vst)
// ============================================================================

// Model sample rate and timing
constexpr float kModelSampleRate_Hz = 16000.0f;
constexpr int kModelFrameSize = 1024;
constexpr int kModelHopSize = 320;
constexpr float kModelInferenceTimerCallbackInterval_ms = 20.0f;
constexpr float kTotalInferenceLatency_ms = 64.0f;

// Model tensor sizes
constexpr int kNoiseAmpsSize = 65;
constexpr int kHarmonicsSize = 60;
constexpr int kAmplitudeSize = 1;
constexpr int kLoudnessSize = 1;
constexpr int kF0Size = 1;
constexpr int kGruModelStateSize = 512;

// Pitch range (MIDI note 0 to 127)
constexpr float kPitchRangeMin_Hz = 8.18f;      // MIDI note 0
constexpr float kPitchRangeMax_Hz = 12543.84f;  // MIDI note 127

// Ring buffer size
constexpr int kRingBufferSize = 61440;

// ============================================================================
// TFLite Tensor Names
// ============================================================================

// Input tensor names
inline constexpr std::string_view kInputTensorName_F0 = "call_f0_scaled:0";
inline constexpr std::string_view kInputTensorName_Loudness = "call_pw_scaled:0";
inline constexpr std::string_view kInputTensorName_State = "call_state:0";

// Output tensor names
inline constexpr std::string_view kOutputTensorName_Amplitude = "StatefulPartitionedCall:0";
inline constexpr std::string_view kOutputTensorName_Harmonics = "StatefulPartitionedCall:1";
inline constexpr std::string_view kOutputTensorName_NoiseAmps = "StatefulPartitionedCall:2";
inline constexpr std::string_view kOutputTensorName_State = "StatefulPartitionedCall:3";

constexpr int kNumPredictControlsInputTensors = 3;
constexpr int kNumPredictControlsOutputTensors = 4;

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Audio features extracted from input or generated from MIDI
 * Used as input to PredictControlsModel
 */
struct AudioFeatures {
    float f0_hz = 0.0f;         // Fundamental frequency in Hz
    float loudness_db = 0.0f;   // Loudness in dB
    float f0_norm = 0.0f;       // Normalized F0 [0, 1]
    float loudness_norm = 0.0f; // Normalized loudness [0, 1]
};

/**
 * Synthesis controls output from PredictControlsModel
 * Used as input to synthesizers
 */
struct SynthesisControls {
    float amplitude = 0.0f;                                  // Overall amplitude
    float f0_hz = 0.0f;                                     // F0 passed through
    std::vector<float> noiseAmps = std::vector<float>(kNoiseAmpsSize, 0.0f);  // Noise magnitudes
    std::vector<float> harmonics = std::vector<float>(kHarmonicsSize, 0.0f);  // Harmonic distribution

    SynthesisControls() = default;

    void clear() {
        amplitude = 0.0f;
        f0_hz = 0.0f;
        std::fill(noiseAmps.begin(), noiseAmps.end(), 0.0f);
        std::fill(harmonics.begin(), harmonics.end(), 0.0f);
    }
};

/**
 * Configuration for DDSP synthesis engine
 */
struct DDSPConfig {
    double sample_rate;          // User/host sample rate (e.g., 44100, 48000)
    int samples_per_block;       // Unity audio buffer size

    std::string model_path;      // Path to .tflite model
    int num_threads;             // Number of threads for TFLite

    // Calculated at runtime based on sample_rate
    int user_frame_size;         // Frame size in user sample rate
    int user_hop_size;           // Hop size in user sample rate

    DDSPConfig()
        : sample_rate(48000.0)
        , samples_per_block(512)
        , num_threads(2)
        , user_frame_size(0)
        , user_hop_size(0)
    {}

    // Calculate user-rate frame/hop sizes
    void updateForSampleRate(double sr) {
        sample_rate = sr;
        // Use ceil for frame size to ensure enough samples
        user_frame_size = static_cast<int>(std::ceil(sr * kModelFrameSize / kModelSampleRate_Hz));
        user_hop_size = static_cast<int>(sr * kModelHopSize / kModelSampleRate_Hz);
    }
};

} // namespace ddsp
