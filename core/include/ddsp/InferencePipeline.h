#pragma once

#include "DDSPTypes.h"
#include "InputUtils.h"
#include "PredictControlsModel.h"
#include "HarmonicSynthesizer.h"
#include "NoiseSynthesizer.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace ddsp {

/**
 * Main DDSP inference and synthesis pipeline
 *
 * Orchestrates the entire DDSP synthesis process:
 * 1. Accept control parameters (f0, loudness, etc.)
 * 2. Run PredictControlsModel inference (on background thread)
 * 3. Generate audio using harmonic + noise synthesizers
 * 4. Provide audio blocks via ring buffer
 *
 * Features:
 * - Sample rate conversion (user rate <-> 16kHz model rate)
 * - Frame-based processing (1024 samples at 16kHz)
 * - Background inference thread
 * - Lock-free ring buffers
 * - Synth mode (MIDI/parameter input, no audio input)
 */
class InferencePipeline {
public:
    explicit InferencePipeline();
    ~InferencePipeline();

    /**
     * Prepare for audio processing
     */
    void prepareToPlay(double sample_rate, int samples_per_block);

    /**
     * Release resources
     */
    void releaseResources();

    /**
     * Load TFLite model
     */
    bool loadModel(const std::string& model_path, int num_threads = 2);

    /**
     * Start/stop background inference thread
     */
    void startTimer(int interval_ms = 20);
    void stopTimer();

    /**
     * Process block (called from audio thread)
     * In synth mode, this enqueues the current parameters for processing
     */
    void processBlock(juce::AudioBuffer<float>& buffer, int num_samples);

    /**
     * Get next synthesized audio block (called from audio thread)
     * @param output Output buffer to fill
     * @param num_samples Number of samples requested
     * @return Number of samples actually written
     */
    int getNextBlock(float* output, int num_samples);

    int getNumReadySamples() const;
    void triggerRender();

    /**
     * Set external control parameters (for synth mode)
     * All parameters are atomic for thread safety
     */
    void setF0Hz(float f0_hz);
    void setLoudnessNorm(float loudness_norm);
    void setLoudnessDb(float loudness_db);

    /**
     * Set pitch shift in semitones
     */
    void setPitchShift(float semitones);

    /**
     * Set output gains
     */
    void setHarmonicGain(float gain);
    void setNoiseGain(float gain);

    /**
     * Reset synthesis state
     */
    void reset();

    /**
     * Check if model is loaded
     */
    bool isReady() const { return model_ready_; }

    /**
     * Get current pitch (for UI feedback)
     */
    float getCurrentPitch() const { return current_pitch_.load(); }

    /**
     * Get current RMS (for UI feedback)
     */
    float getCurrentRMS() const { return current_rms_.load(); }

private:
    // Configuration
    double sample_rate_;
    int samples_per_block_;
    int user_frame_size_;
    int user_hop_size_;
    bool model_ready_;

    // Core components
    std::unique_ptr<PredictControlsModel> model_;
    std::unique_ptr<HarmonicSynthesizer> harmonic_synth_;
    std::unique_ptr<NoiseSynthesizer> noise_synth_;

    // Ring buffers (using JUCE AbstractFifo)
    std::unique_ptr<juce::AbstractFifo> input_fifo_;
    std::unique_ptr<juce::AbstractFifo> output_fifo_;
    juce::AudioBuffer<float> input_ring_buffer_;
    juce::AudioBuffer<float> output_ring_buffer_;

    // Resampling (JUCE interpolators)
    juce::WindowedSincInterpolator input_interpolator_;
    juce::WindowedSincInterpolator output_interpolator_;

    // Working buffers
    juce::AudioBuffer<float> model_input_buffer_;           // User sample rate frame
    juce::AudioBuffer<float> resampled_model_input_buffer_; // 16kHz frame (1024)
    juce::AudioBuffer<float> synthesis_buffer_;              // 16kHz hop (320)
    juce::AudioBuffer<float> resampled_model_output_buffer_; // User sample rate hop

    // Control parameters (atomic for thread safety)
    std::atomic<float> f0_hz_;
    std::atomic<float> loudness_norm_;
    std::atomic<float> pitch_shift_semitones_;
    std::atomic<float> harmonic_gain_;
    std::atomic<float> noise_gain_;

    // Current state for UI feedback
    std::atomic<float> current_pitch_;
    std::atomic<float> current_rms_;

    // Current control data
    AudioFeatures predict_controls_input_;
    SynthesisControls synthesis_input_;

    // Background thread
    std::atomic<bool> should_run_;
    std::unique_ptr<std::thread> render_thread_;

    /**
     * Background rendering loop
     */
    void renderLoop(int interval_ms);

    /**
     * Single render iteration
     * Called from background thread
     */
    void render();

    /**
     * Push samples to input ring buffer
     */
    void pushToInputBuffer(const juce::AudioBuffer<float>& buffer);

    /**
     * Pop samples from output ring buffer
     */
    int popFromOutputBuffer(float* output, int num_samples);

    /**
     * Zero-pad input buffer for latency compensation
     */
    void zeroPadInputBuffer();
};

} // namespace ddsp
