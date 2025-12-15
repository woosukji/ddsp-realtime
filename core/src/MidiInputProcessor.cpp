#include "MidiInputProcessor.h"
#include <cmath>

namespace ddsp {

MidiInputProcessor::MidiInputProcessor()
    : sample_rate_(48000.0)
    , hop_size_(960)  // Default for 48kHz
    , current_midi_note_(69)  // A4
    , current_pitch_bend_(8192)  // Center
    , current_midi_velocity_(0.0f)  // Off
{
    // Default ADSR parameters
    adsr_params_.attack = 0.01f;
    adsr_params_.decay = 0.1f;
    adsr_params_.sustain = 0.7f;
    adsr_params_.release = 0.2f;

    adsr_.setParameters(adsr_params_);
}

void MidiInputProcessor::prepareToPlay(double sample_rate, int hop_size) {
    sample_rate_ = sample_rate;
    hop_size_ = hop_size;

    adsr_.setSampleRate(sample_rate);
    adsr_.setParameters(adsr_params_);
}

void MidiInputProcessor::processMidiBuffer(const juce::MidiBuffer& midi_buffer) {
    for (const auto metadata : midi_buffer) {
        const auto message = metadata.getMessage();

        if (message.isNoteOn()) {
            noteOn(message.getNoteNumber(), message.getFloatVelocity());
        }
        else if (message.isNoteOff()) {
            noteOff();
        }
        else if (message.isPitchWheel()) {
            setPitchBend(message.getPitchWheelValue());
        }
    }
}

AudioFeatures MidiInputProcessor::getCurrentPredictControlsInput() {
    AudioFeatures features;

    // Convert MIDI note + pitch bend to frequency
    int midi_note = current_midi_note_.load(std::memory_order_acquire);
    int pitch_bend = current_pitch_bend_.load(std::memory_order_acquire);
    float velocity = current_midi_velocity_.load(std::memory_order_acquire);

    float f0_hz = getFreqFromNoteAndBend(midi_note, pitch_bend);

    // Use mapFromLog10 for MIDI mode (different from audio mode)
    float f0_norm = mapFromLog10(f0_hz);

    features.f0_hz = f0_hz;
    features.f0_norm = f0_norm;

    // Process ADSR envelope for one hop's worth of samples
    float loudness_norm = 0.0f;
    for (int i = 0; i < hop_size_; ++i) {
        loudness_norm = adsr_.getNextSample() * velocity;
    }

    features.loudness_norm = loudness_norm;
    features.loudness_db = denormalizeLoudness(loudness_norm);

    return features;
}

void MidiInputProcessor::setADSR(float attack_sec, float decay_sec, float sustain_level, float release_sec) {
    adsr_params_.attack = attack_sec;
    adsr_params_.decay = decay_sec;
    adsr_params_.sustain = sustain_level;
    adsr_params_.release = release_sec;

    adsr_.setParameters(adsr_params_);
}

void MidiInputProcessor::reset() {
    adsr_.reset();
    current_midi_note_.store(69, std::memory_order_release);
    current_pitch_bend_.store(8192, std::memory_order_release);
    current_midi_velocity_.store(0.0f, std::memory_order_release);
}

void MidiInputProcessor::noteOn(int midi_note, float velocity) {
    current_midi_note_.store(midi_note, std::memory_order_release);
    current_midi_velocity_.store(velocity, std::memory_order_release);
    adsr_.noteOn();
}

void MidiInputProcessor::noteOff() {
    adsr_.noteOff();
}

void MidiInputProcessor::setPitchBend(int pitch_bend) {
    current_pitch_bend_.store(pitch_bend, std::memory_order_release);
}

} // namespace ddsp
