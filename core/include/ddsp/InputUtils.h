#pragma once

#include "DDSPTypes.h"
#include <cmath>
#include <algorithm>

namespace ddsp {

// ============================================================================
// Pitch and Loudness Constants
// ============================================================================

static constexpr float kSemitonesPerOctave = 12.0f;
static constexpr float kMidiNoteA4 = 69.0f;
static constexpr float kFreqA4_Hz = 440.0f;

// Pitch bend constants
static constexpr float kPitchBendRange = 16384.0f;
static constexpr float kPitchBendBase = kPitchBendRange / 2.0f;  // 8192
static constexpr float kPitchRangePerSemitone = kPitchBendRange / 4.0f;  // +-2 semitones

// ============================================================================
// Pitch Normalization
// ============================================================================

/**
 * Normalize pitch from Hz to [0, 1] range
 * Uses MIDI scale: Hz -> MIDI note [0, 127] -> [0, 1]
 *
 * @param pitch_Hz Frequency in Hz
 * @return Normalized pitch [0, 1]
 */
static inline float normalizedPitch(float pitch_Hz) {
    // Clamp to valid pitch range
    pitch_Hz = std::clamp(pitch_Hz, kPitchRangeMin_Hz, kPitchRangeMax_Hz);

    // Convert to MIDI scale: midi = 12 * (log2(f) - log2(440)) + 69
    float midi = kSemitonesPerOctave * (std::log2(pitch_Hz) - std::log2(kFreqA4_Hz)) + kMidiNoteA4;

    // Normalize: [0, 127] -> [0, 1]
    return midi / 127.0f;
}

/**
 * Apply semitone offset to pitch
 *
 * @param pitch_Hz Original frequency in Hz
 * @param semitone_offset Number of semitones to shift (can be negative)
 * @return Shifted frequency in Hz
 */
static inline float offsetPitch(float pitch_Hz, float semitone_offset) {
    return pitch_Hz * std::pow(2.0f, semitone_offset / kSemitonesPerOctave);
}

/**
 * Convert MIDI note and pitch bend to frequency
 *
 * @param midi_note MIDI note number (0-127)
 * @param pitch_bend Raw pitch bend value (0-16383, center at 8192)
 * @return Frequency in Hz
 */
static inline float getFreqFromNoteAndBend(int midi_note, int pitch_bend) {
    const float noteInOctave = (midi_note - kMidiNoteA4) / kSemitonesPerOctave;
    const float pitchBendInOctave = (pitch_bend - kPitchBendBase) / kPitchRangePerSemitone / kSemitonesPerOctave;
    const float f0_Hz = std::pow(2.0f, noteInOctave + pitchBendInOctave) * kFreqA4_Hz;
    return f0_Hz;
}

/**
 * Convert frequency to MIDI note number (float, for pitch bend)
 *
 * @param freq_Hz Frequency in Hz
 * @return MIDI note number (float)
 */
static inline float freqToMidiNote(float freq_Hz) {
    return kSemitonesPerOctave * (std::log2(freq_Hz) - std::log2(kFreqA4_Hz)) + kMidiNoteA4;
}

/**
 * Logarithmic mapping from Hz to [0, 1]
 * Used in MIDI/synth mode (different from normalizedPitch)
 *
 * @param freq_Hz Frequency in Hz
 * @return Normalized value [0, 1]
 */
static inline float mapFromLog10(float freq_Hz) {
    freq_Hz = std::clamp(freq_Hz, kPitchRangeMin_Hz, kPitchRangeMax_Hz);

    float logMin = std::log10(kPitchRangeMin_Hz);
    float logMax = std::log10(kPitchRangeMax_Hz);
    float logValue = std::log10(freq_Hz);

    return (logValue - logMin) / (logMax - logMin);
}

// ============================================================================
// Loudness Normalization
// ============================================================================

/**
 * Normalize loudness from dB to [0, 1] range
 * Uses 80dB range for parity with DDSP Python normalization
 *
 * @param loudness_dB Loudness in dB (typically -80 to 0)
 * @return Normalized loudness [0, 1]
 */
static inline float normalizedLoudness(float loudness_dB) {
    return (loudness_dB / 80.0f) + 1.0f;
}

/**
 * Denormalize loudness from [0, 1] to dB
 *
 * @param loudness_norm Normalized loudness [0, 1]
 * @return Loudness in dB
 */
static inline float denormalizeLoudness(float loudness_norm) {
    return (loudness_norm - 1.0f) * 80.0f;
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Linear interpolation
 */
static inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/**
 * Map value from one range to another
 */
static inline float mapValue(float value, float inMin, float inMax, float outMin, float outMax) {
    return outMin + (outMax - outMin) * (value - inMin) / (inMax - inMin);
}

/**
 * dB to linear amplitude
 */
static inline float dbToLinear(float dB) {
    return std::pow(10.0f, dB / 20.0f);
}

/**
 * Linear amplitude to dB
 */
static inline float linearToDb(float linear) {
    return 20.0f * std::log10(std::max(linear, 1e-10f));
}

} // namespace ddsp
