#pragma once

#include "DDSPTypes.h"
#include "InputUtils.h"
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>

namespace ddsp {

/**
 * Processes MIDI input for synth mode
 *
 * Converts MIDI note/velocity/pitch bend to AudioFeatures
 * with ADSR envelope for loudness.
 */
class MidiInputProcessor {
public:
    MidiInputProcessor();
    ~MidiInputProcessor() = default;

    /**
     * Prepare for processing
     */
    void prepareToPlay(double sample_rate, int hop_size);

    /**
     * Process MIDI buffer
     */
    void processMidiBuffer(const juce::MidiBuffer& midi_buffer);

    /**
     * Get current AudioFeatures (for synth mode)
     * Processes ADSR envelope for one hop's worth of samples
     */
    AudioFeatures getCurrentPredictControlsInput();

    /**
     * Set ADSR parameters
     */
    void setADSR(float attack_sec, float decay_sec, float sustain_level, float release_sec);

    /**
     * Reset state
     */
    void reset();

    /**
     * Note on/off
     */
    void noteOn(int midi_note, float velocity);
    void noteOff();

    /**
     * Pitch bend
     */
    void setPitchBend(int pitch_bend);

private:
    double sample_rate_;
    int hop_size_;

    // Current MIDI state (atomic for thread safety)
    std::atomic<int> current_midi_note_;
    std::atomic<int> current_pitch_bend_;
    std::atomic<float> current_midi_velocity_;

    // ADSR envelope
    juce::ADSR adsr_;
    juce::ADSR::Parameters adsr_params_;
};

} // namespace ddsp
