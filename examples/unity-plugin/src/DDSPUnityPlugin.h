#pragma once

#include "AudioPluginInterface.h"
#include "InferencePipeline.h"
#include "DDSPTypes.h"
#include <memory>
#include <cstring>

namespace ddsp_unity {

/**
 * Plugin parameter indices
 */
enum Param {
    P_F0 = 0,           // Fundamental frequency (Hz)
    P_LOUDNESS,         // Normalized loudness (0-1)
    P_PITCH_SHIFT,      // Pitch shift (semitones)
    P_HARMONIC_GAIN,    // Harmonic gain
    P_NOISE_GAIN,       // Noise gain
    P_OUTPUT_GAIN,      // Output gain (dB)
    P_NUM               // Total number of parameters
};

/**
 * Plugin internal state data
 * Wraps DDSP InferencePipeline
 */
struct DDSPPluginState {
    std::unique_ptr<ddsp::InferencePipeline> pipeline;
    std::vector<float> temp_buffer;
    bool initialized;

    DDSPPluginState(double sample_rate, int buffer_size);
    ~DDSPPluginState() = default;
};

/**
 * Effect data structure
 * Must be aligned for Unity's requirements
 */
struct EffectData {
    struct Data {
        float p[P_NUM];           // Parameter values
        DDSPPluginState* state;   // DDSP state
    };

    union {
        Data data;
        unsigned char pad[(sizeof(Data) + 15) & ~15];  // 16-byte alignment
    };
};

// Plugin callbacks
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state);
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state);
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ResetCallback(UnityAudioEffectState* state);

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(
    UnityAudioEffectState* state,
    float* inbuffer,
    float* outbuffer,
    unsigned int length,
    int inchannels,
    int outchannels);

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(
    UnityAudioEffectState* state, int index, float value);

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(
    UnityAudioEffectState* state, int index, float* value, char* valuestr);

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(
    UnityAudioEffectState* state, const char* name, float* buffer, int numsamples);

// Helper functions
void InternalRegisterEffectDefinition(UnityAudioEffectDefinition& def);
void InitParametersFromDefinitions(
    void (*regFunc)(UnityAudioEffectDefinition&),
    float* params);

} // namespace ddsp_unity
