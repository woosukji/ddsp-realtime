#include "InferencePipeline.h"
#include <iostream>
#include <chrono>
#include <cmath>

namespace ddsp {

InferencePipeline::InferencePipeline()
    : sample_rate_(48000.0)
    , samples_per_block_(512)
    , user_frame_size_(0)
    , user_hop_size_(0)
    , model_ready_(false)
    , f0_hz_(440.0f)
    , loudness_norm_(0.5f)
    , pitch_shift_semitones_(0.0f)
    , harmonic_gain_(1.0f)
    , noise_gain_(1.0f)
    , current_pitch_(0.0f)
    , current_rms_(0.0f)
    , should_run_(false)
{
    // Create model
    model_ = std::make_unique<PredictControlsModel>();

    // Create synthesizers at model sample rate
    harmonic_synth_ = std::make_unique<HarmonicSynthesizer>(
        kHarmonicsSize, kModelHopSize, kModelSampleRate_Hz);
    noise_synth_ = std::make_unique<NoiseSynthesizer>(
        kNoiseAmpsSize, kModelHopSize);
}

InferencePipeline::~InferencePipeline() {
    stopTimer();
    releaseResources();
}

void InferencePipeline::prepareToPlay(double sample_rate, int samples_per_block) {
    sample_rate_ = sample_rate;
    samples_per_block_ = samples_per_block;

    // Calculate user-rate frame/hop sizes
    user_frame_size_ = static_cast<int>(std::ceil(sample_rate * kModelFrameSize / kModelSampleRate_Hz));
    user_hop_size_ = static_cast<int>(sample_rate * kModelHopSize / kModelSampleRate_Hz);

    // Create ring buffers
    input_fifo_ = std::make_unique<juce::AbstractFifo>(kRingBufferSize);
    output_fifo_ = std::make_unique<juce::AbstractFifo>(kRingBufferSize);
    input_ring_buffer_.setSize(1, kRingBufferSize);
    output_ring_buffer_.setSize(1, kRingBufferSize);

    // Allocate working buffers
    model_input_buffer_.setSize(1, user_frame_size_);
    resampled_model_input_buffer_.setSize(1, kModelFrameSize);
    synthesis_buffer_.setSize(1, kModelHopSize);
    resampled_model_output_buffer_.setSize(1, user_hop_size_);

    // Reset everything
    reset();
}

void InferencePipeline::releaseResources() {
    model_ready_ = false;
}

bool InferencePipeline::loadModel(const std::string& model_path, int num_threads) {
    if (!model_->loadModel(model_path, num_threads)) {
        std::cerr << "Failed to load DDSP model" << std::endl;
        return false;
    }

    model_ready_ = true;
    return true;
}

void InferencePipeline::startTimer(int interval_ms) {
    if (should_run_.load()) {
        return;  // Already running
    }

    should_run_.store(true);

    // Start background rendering thread
    render_thread_ = std::make_unique<std::thread>([this, interval_ms]() {
        renderLoop(interval_ms);
    });
}

void InferencePipeline::stopTimer() {
    should_run_.store(false);

    if (render_thread_ && render_thread_->joinable()) {
        render_thread_->join();
    }
    render_thread_.reset();
}

void InferencePipeline::processBlock(juce::AudioBuffer<float>& buffer, int num_samples) {
    // In synth mode, we don't use input audio
    // Just trigger processing by updating the input buffer

    // For synth mode with no audio input, we don't need to push anything
    // The render() function uses the atomic parameters directly
}

int InferencePipeline::getNextBlock(float* output, int num_samples) {
    return popFromOutputBuffer(output, num_samples);
}

void InferencePipeline::setF0Hz(float f0_hz) {
    f0_hz_.store(std::clamp(f0_hz, kPitchRangeMin_Hz, kPitchRangeMax_Hz));
}

void InferencePipeline::setLoudnessNorm(float loudness_norm) {
    loudness_norm_.store(std::clamp(loudness_norm, 0.0f, 1.0f));
}

void InferencePipeline::setLoudnessDb(float loudness_db) {
    float norm = normalizedLoudness(loudness_db);
    loudness_norm_.store(std::clamp(norm, 0.0f, 1.0f));
}

void InferencePipeline::setPitchShift(float semitones) {
    pitch_shift_semitones_.store(semitones);
}

void InferencePipeline::setHarmonicGain(float gain) {
    harmonic_gain_.store(std::clamp(gain, 0.0f, 10.0f));
}

void InferencePipeline::setNoiseGain(float gain) {
    noise_gain_.store(std::clamp(gain, 0.0f, 10.0f));
}

void InferencePipeline::reset() {
    // Reset model
    if (model_) {
        model_->reset();
    }

    // Reset synthesizers
    if (harmonic_synth_) {
        harmonic_synth_->reset();
    }
    if (noise_synth_) {
        noise_synth_->reset();
    }

    // Clear buffers
    model_input_buffer_.clear();
    resampled_model_input_buffer_.clear();
    synthesis_buffer_.clear();
    resampled_model_output_buffer_.clear();

    // Clear ring buffers
    if (input_fifo_) {
        input_fifo_->reset();
    }
    if (output_fifo_) {
        output_fifo_->reset();
    }
    input_ring_buffer_.clear();
    output_ring_buffer_.clear();

    // Reset interpolators
    input_interpolator_.reset();
    output_interpolator_.reset();

    // Zero-pad input buffer for latency compensation
    zeroPadInputBuffer();
}

void InferencePipeline::zeroPadInputBuffer() {
    if (user_frame_size_ <= 0 || !input_fifo_) {
        return;
    }

    // Push zeros to input buffer
    juce::AudioBuffer<float> zero_buffer(1, user_frame_size_);
    zero_buffer.clear();
    pushToInputBuffer(zero_buffer);
}

void InferencePipeline::pushToInputBuffer(const juce::AudioBuffer<float>& buffer) {
    if (!input_fifo_) return;

    int num_samples = buffer.getNumSamples();
    const float* src = buffer.getReadPointer(0);
    float* dst = input_ring_buffer_.getWritePointer(0);

    int start1, size1, start2, size2;
    input_fifo_->prepareToWrite(num_samples, start1, size1, start2, size2);

    if (size1 > 0) {
        std::copy(src, src + size1, dst + start1);
    }
    if (size2 > 0) {
        std::copy(src + size1, src + size1 + size2, dst + start2);
    }

    input_fifo_->finishedWrite(size1 + size2);
}

int InferencePipeline::popFromOutputBuffer(float* output, int num_samples) {
    if (!output_fifo_) return 0;

    const float* src = output_ring_buffer_.getReadPointer(0);

    int start1, size1, start2, size2;
    output_fifo_->prepareToRead(num_samples, start1, size1, start2, size2);

    int total_read = size1 + size2;

    if (size1 > 0) {
        std::copy(src + start1, src + start1 + size1, output);
    }
    if (size2 > 0) {
        std::copy(src + start2, src + start2 + size2, output + size1);
    }

    output_fifo_->finishedRead(total_read);

    // Fill remaining with silence
    if (total_read < num_samples) {
        std::fill(output + total_read, output + num_samples, 0.0f);
    }

    return total_read;
}

void InferencePipeline::renderLoop(int interval_ms) {
    while (should_run_.load()) {
        auto start = std::chrono::steady_clock::now();

        // Run one render iteration
        render();

        // Sleep for remaining time
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto sleep_time = std::chrono::milliseconds(interval_ms) - elapsed;

        if (sleep_time.count() > 0) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
}

void InferencePipeline::render() {
    if (!model_ready_) {
        return;
    }

    // --- SYNTH MODE: Get F0/loudness from parameters ---
    float f0_hz = f0_hz_.load();
    float loudness_norm = loudness_norm_.load();
    float pitch_shift = pitch_shift_semitones_.load();

    // Apply pitch shift
    f0_hz = offsetPitch(f0_hz, pitch_shift);

    // Calculate normalized F0
    float f0_norm = normalizedPitch(f0_hz);

    // Store for UI feedback
    current_pitch_.store(f0_norm);
    current_rms_.store(loudness_norm);

    // Build AudioFeatures input
    predict_controls_input_.f0_hz = f0_hz;
    predict_controls_input_.f0_norm = f0_norm;
    predict_controls_input_.loudness_norm = loudness_norm;
    predict_controls_input_.loudness_db = denormalizeLoudness(loudness_norm);

    // --- RUN MODEL INFERENCE ---
    if (!model_->call(predict_controls_input_, synthesis_input_)) {
        std::cerr << "Inference failed" << std::endl;
        return;
    }

    // --- APPLY OUTPUT GAINS ---
    float harm_gain = harmonic_gain_.load();
    float noise_gain = noise_gain_.load();

    synthesis_input_.amplitude *= harm_gain;
    for (auto& amp : synthesis_input_.noiseAmps) {
        amp *= noise_gain;
    }

    // --- SYNTHESIZE AUDIO ---
    const auto& harmonic_output = harmonic_synth_->render(
        synthesis_input_.harmonics,
        synthesis_input_.amplitude,
        synthesis_input_.f0_hz
    );

    const auto& noise_output = noise_synth_->render(synthesis_input_.noiseAmps);

    // --- MIX HARMONIC + NOISE ---
    float* synthesis_ptr = synthesis_buffer_.getWritePointer(0);
    for (int i = 0; i < kModelHopSize; ++i) {
        synthesis_ptr[i] = harmonic_output[i] + noise_output[i];
    }

    // --- UPSAMPLE TO USER SAMPLE RATE ---
    float* output_ptr = resampled_model_output_buffer_.getWritePointer(0);
    output_interpolator_.process(
        kModelSampleRate_Hz / sample_rate_,  // Inverse ratio for upsampling
        synthesis_ptr,
        output_ptr,
        resampled_model_output_buffer_.getNumSamples()
    );

    // --- PUSH TO OUTPUT RING BUFFER ---
    if (output_fifo_) {
        float* dst = output_ring_buffer_.getWritePointer(0);

        int start1, size1, start2, size2;
        output_fifo_->prepareToWrite(user_hop_size_, start1, size1, start2, size2);

        if (size1 > 0) {
            std::copy(output_ptr, output_ptr + size1, dst + start1);
        }
        if (size2 > 0) {
            std::copy(output_ptr + size1, output_ptr + size1 + size2, dst + start2);
        }

        output_fifo_->finishedWrite(size1 + size2);
    }
}

int InferencePipeline::getNumReadySamples() const {
    return output_fifo_ ? output_fifo_->getNumReady() : 0;
}

void InferencePipeline::triggerRender() {
    render();
}

} // namespace ddsp
