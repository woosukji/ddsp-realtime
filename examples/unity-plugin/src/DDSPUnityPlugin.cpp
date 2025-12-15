#include "DDSPUnityPlugin.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace ddsp_unity {

// ============================================================================
// DDSPPluginState Implementation
// ============================================================================

DDSPPluginState::DDSPPluginState(double sample_rate, int buffer_size)
    : initialized(false)
{
    // Create inference pipeline
    pipeline = std::make_unique<ddsp::InferencePipeline>();
    pipeline->prepareToPlay(sample_rate, buffer_size);

    // Allocate temp buffer
    temp_buffer.resize(buffer_size, 0.0f);

    initialized = true;
}

// ============================================================================
// Parameter Definitions
// ============================================================================

void InternalRegisterEffectDefinition(UnityAudioEffectDefinition& def) {
    // F0 (fundamental frequency)
    std::strncpy(def.paramdefs[P_F0].name, "F0", 15);
    std::strncpy(def.paramdefs[P_F0].unit, "Hz", 15);
    def.paramdefs[P_F0].description = "Fundamental frequency";
    def.paramdefs[P_F0].min = 440.0f;
    def.paramdefs[P_F0].max = 660.0f;
    def.paramdefs[P_F0].defaultval = 440.0f;
    def.paramdefs[P_F0].displayscale = 1.0f;
    def.paramdefs[P_F0].displayexponent = 1.0f;

    // Loudness (normalized)
    std::strncpy(def.paramdefs[P_LOUDNESS].name, "Loudness", 15);
    std::strncpy(def.paramdefs[P_LOUDNESS].unit, "", 15);
    def.paramdefs[P_LOUDNESS].description = "Normalized loudness";
    def.paramdefs[P_LOUDNESS].min = 0.0f;
    def.paramdefs[P_LOUDNESS].max = 1.0f;
    def.paramdefs[P_LOUDNESS].defaultval = 0.5f;
    def.paramdefs[P_LOUDNESS].displayscale = 100.0f;
    def.paramdefs[P_LOUDNESS].displayexponent = 1.0f;

    // Pitch shift
    std::strncpy(def.paramdefs[P_PITCH_SHIFT].name, "PitchShift", 15);
    std::strncpy(def.paramdefs[P_PITCH_SHIFT].unit, "st", 15);
    def.paramdefs[P_PITCH_SHIFT].description = "Pitch shift in semitones";
    def.paramdefs[P_PITCH_SHIFT].min = -24.0f;
    def.paramdefs[P_PITCH_SHIFT].max = 24.0f;
    def.paramdefs[P_PITCH_SHIFT].defaultval = 0.0f;
    def.paramdefs[P_PITCH_SHIFT].displayscale = 1.0f;
    def.paramdefs[P_PITCH_SHIFT].displayexponent = 1.0f;

    // Harmonic gain
    std::strncpy(def.paramdefs[P_HARMONIC_GAIN].name, "HarmGain", 15);
    std::strncpy(def.paramdefs[P_HARMONIC_GAIN].unit, "", 15);
    def.paramdefs[P_HARMONIC_GAIN].description = "Harmonic gain";
    def.paramdefs[P_HARMONIC_GAIN].min = 0.0f;
    def.paramdefs[P_HARMONIC_GAIN].max = 2.0f;
    def.paramdefs[P_HARMONIC_GAIN].defaultval = 1.0f;
    def.paramdefs[P_HARMONIC_GAIN].displayscale = 100.0f;
    def.paramdefs[P_HARMONIC_GAIN].displayexponent = 1.0f;

    // Noise gain
    std::strncpy(def.paramdefs[P_NOISE_GAIN].name, "NoiseGain", 15);
    std::strncpy(def.paramdefs[P_NOISE_GAIN].unit, "", 15);
    def.paramdefs[P_NOISE_GAIN].description = "Noise gain";
    def.paramdefs[P_NOISE_GAIN].min = 0.0f;
    def.paramdefs[P_NOISE_GAIN].max = 2.0f;
    def.paramdefs[P_NOISE_GAIN].defaultval = 1.0f;
    def.paramdefs[P_NOISE_GAIN].displayscale = 100.0f;
    def.paramdefs[P_NOISE_GAIN].displayexponent = 1.0f;

    // Output gain
    std::strncpy(def.paramdefs[P_OUTPUT_GAIN].name, "OutGain", 15);
    std::strncpy(def.paramdefs[P_OUTPUT_GAIN].unit, "dB", 15);
    def.paramdefs[P_OUTPUT_GAIN].description = "Output gain";
    def.paramdefs[P_OUTPUT_GAIN].min = -60.0f;
    def.paramdefs[P_OUTPUT_GAIN].max = 12.0f;
    def.paramdefs[P_OUTPUT_GAIN].defaultval = 0.0f;
    def.paramdefs[P_OUTPUT_GAIN].displayscale = 1.0f;
    def.paramdefs[P_OUTPUT_GAIN].displayexponent = 1.0f;

    def.numparameters = P_NUM;
}

void InitParametersFromDefinitions(
    void (*regFunc)(UnityAudioEffectDefinition&),
    float* params)
{
    UnityAudioEffectDefinition def;
    memset(&def, 0, sizeof(def));

    UnityAudioParameterDefinition paramdefs[P_NUM];
    memset(paramdefs, 0, sizeof(paramdefs));
    def.paramdefs = paramdefs;

    regFunc(def);

    for (int i = 0; i < P_NUM; ++i) {
        params[i] = def.paramdefs[i].defaultval;
    }
}

// ============================================================================
// Unity Callbacks
// ============================================================================

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state) {
    // temporary debug
    std::fprintf(stderr, "[DDSP] CreateCallback entered\n");
    std::fflush(stderr);

    auto* effect = new EffectData;
    memset(effect, 0, sizeof(EffectData));
    state->effectdata = effect;

    // Initialize parameters with defaults
    InitParametersFromDefinitions(InternalRegisterEffectDefinition, effect->data.p);

    // Create DDSP state
    double sample_rate = state->samplerate;
    int buffer_size = state->dspbuffersize;

    effect->data.state = new DDSPPluginState(sample_rate, buffer_size);

    // Load model from environment variable or default path
    const char* model_path_env = std::getenv("DDSP_MODEL_PATH");
    std::string model_path;

    if (model_path_env && model_path_env[0] != '\0') {
        // Use environment variable if set
        model_path = model_path_env;
    } else {
        // Use default relative path (assumes models/ is at project root)
        model_path = "../../models/Violin.tflite";
    }

    if (!model_path.empty()) {
        effect->data.state->pipeline->loadModel(model_path);
    }

    // Start background rendering
    effect->data.state->pipeline->startTimer(20);  // 20ms interval

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state) {
    auto* effect = state->GetEffectData<EffectData>();
    if (!effect) {
        return UNITY_AUDIODSP_OK;
    }

    if (effect->data.state) {
        effect->data.state->pipeline->stopTimer();
        delete effect->data.state;
    }

    delete effect;
    state->effectdata = nullptr;

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ResetCallback(UnityAudioEffectState* state) {
    auto* effect = state->GetEffectData<EffectData>();
    if (effect && effect->data.state && effect->data.state->pipeline) {
        effect->data.state->pipeline->reset();
    }
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(
    UnityAudioEffectState* state,
    float* inbuffer,
    float* outbuffer,
    unsigned int length,
    int inchannels,
    int outchannels)
{
    auto* effect = state->GetEffectData<EffectData>();
    if (!effect || !effect->data.state || !effect->data.state->initialized) {
        // Not ready - output silence
        memset(outbuffer, 0, sizeof(float) * length * outchannels);
        return UNITY_AUDIODSP_OK;
    }

    auto* ddsp_state = effect->data.state;
    auto* pipeline = ddsp_state->pipeline.get();

    // Get next output block (mono)
    int samples_read = pipeline->getNextBlock(
        ddsp_state->temp_buffer.data(),
        length);

    // Apply output gain
    float gain_db = effect->data.p[P_OUTPUT_GAIN];
    float gain_linear = std::pow(10.0f, gain_db / 20.0f);

    // Copy mono output to all channels
    const float* mono = ddsp_state->temp_buffer.data();

    for (unsigned int n = 0; n < length; ++n) {
        float sample = (n < (unsigned int)samples_read) ? mono[n] * gain_linear : 0.0f;

        for (int ch = 0; ch < outchannels; ++ch) {
            outbuffer[n * outchannels + ch] = sample;
        }
    }

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(
    UnityAudioEffectState* state, int index, float value)
{
    auto* effect = state->GetEffectData<EffectData>();
    if (!effect) {
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;
    }

    if (index < 0 || index >= P_NUM) {
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;
    }

    effect->data.p[index] = value;

    // Update DDSP pipeline
    if (effect->data.state && effect->data.state->pipeline) {
        auto* pipeline = effect->data.state->pipeline.get();

        switch (index) {
            case P_F0:
                pipeline->setF0Hz(value);
                break;
            case P_LOUDNESS:
                pipeline->setLoudnessNorm(value);
                break;
            case P_PITCH_SHIFT:
                pipeline->setPitchShift(value);
                break;
            case P_HARMONIC_GAIN:
                pipeline->setHarmonicGain(value);
                break;
            case P_NOISE_GAIN:
                pipeline->setNoiseGain(value);
                break;
            case P_OUTPUT_GAIN:
                // Handled in ProcessCallback
                break;
        }
    }

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(
    UnityAudioEffectState* state, int index, float* value, char* valuestr)
{
    auto* effect = state->GetEffectData<EffectData>();
    if (!effect) {
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;
    }

    if (index < 0 || index >= P_NUM) {
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;
    }

    if (value) {
        *value = effect->data.p[index];
    }

    if (valuestr) {
        valuestr[0] = 0;  // Leave empty for Unity to format
    }

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(
    UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
{
    // Not used for this plugin
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

} // namespace ddsp_unity

// ============================================================================
// Unity Plugin Entry Point
// ============================================================================

extern "C" UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(
    UnityAudioEffectDefinition*** definitionsptr)
{
    static UnityAudioParameterDefinition paramdefs[ddsp_unity::P_NUM];
    static UnityAudioEffectDefinition effectdef;

    memset(paramdefs, 0, sizeof(paramdefs));
    memset(&effectdef, 0, sizeof(effectdef));

    effectdef.structsize = sizeof(UnityAudioEffectDefinition);
    effectdef.paramstructsize = sizeof(UnityAudioParameterDefinition);
    effectdef.apiversion = 0x010300;  // Unity 2019.x+
    effectdef.pluginversion = 0x010000;

    std::strncpy(effectdef.name, "DDSP Synth", sizeof(effectdef.name) - 1);
    effectdef.name[sizeof(effectdef.name) - 1] = '\0';

    effectdef.channels = 0;
    effectdef.numparameters = ddsp_unity::P_NUM;
    effectdef.flags = 0;

    effectdef.create = ddsp_unity::CreateCallback;
    effectdef.release = ddsp_unity::ReleaseCallback;
    effectdef.reset = ddsp_unity::ResetCallback;
    effectdef.process = ddsp_unity::ProcessCallback;
    effectdef.setposition = nullptr;
    effectdef.setfloatparameter = ddsp_unity::SetFloatParameterCallback;
    effectdef.getfloatparameter = ddsp_unity::GetFloatParameterCallback;
    effectdef.getfloatbuffer = ddsp_unity::GetFloatBufferCallback;

    effectdef.paramdefs = paramdefs;
    ddsp_unity::InternalRegisterEffectDefinition(effectdef);

    static UnityAudioEffectDefinition* defptr = &effectdef;
    *definitionsptr = &defptr;

    return 1;  // Number of effects in this plugin
}
