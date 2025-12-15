#pragma once

#include "DDSPTypes.h"
#include <string>
#include <array>
#include <unordered_map>

// Forward declarations for TFLite C types
struct TfLiteModel;
struct TfLiteInterpreterOptions;
struct TfLiteInterpreter;
struct TfLiteDelegate;

namespace ddsp {

/**
 * Wraps TensorFlow Lite model for DDSP control prediction
 *
 * Takes normalized audio features (f0, loudness) and predicts
 * synthesis controls (amplitude, harmonics, noise magnitudes).
 *
 * Key features:
 * - Uses tensor names for matching (order-agnostic)
 * - Maintains GRU state between frames
 * - Supports XNNPACK and CoreML delegates
 *
 * Thread-safety: NOT thread-safe. Use from single thread only.
 */
class PredictControlsModel {
public:
    PredictControlsModel();
    ~PredictControlsModel();

    // Delete copy/move (TFLite interpreter shouldn't be copied)
    PredictControlsModel(const PredictControlsModel&) = delete;
    PredictControlsModel& operator=(const PredictControlsModel&) = delete;

    /**
     * Load TFLite model from file
     * @param model_path Path to .tflite model file
     * @param num_threads Number of threads for inference
     * @return true if successful
     */
    bool loadModel(const std::string& model_path, int num_threads = 2);

    /**
     * Run inference
     *
     * @param input Audio features (f0_norm, loudness_norm must be set)
     * @param output Synthesis controls (amplitude, harmonics, noiseAmps)
     * @return true if inference successful
     */
    bool call(const AudioFeatures& input, SynthesisControls& output);

    /**
     * Reset model state (clears GRU state)
     */
    void reset();

    /**
     * Check if model is loaded and ready
     */
    bool isLoaded() const { return model_loaded_; }

private:
    bool model_loaded_;

    enum class DelegateType {
        None,
        CoreML,
        XNNPACK
    };

    // TFLite objects
    TfLiteModel* model_;
    TfLiteInterpreterOptions* interpreter_options_;
    TfLiteInterpreter* interpreter_;
    TfLiteDelegate* delegate_;
    DelegateType delegate_type_;

    std::unordered_map<std::string, int> input_indices_;
    std::unordered_map<std::string, int> output_indices_;

    // GRU state (512 floats, persists between frames)
    std::array<float, kGruModelStateSize> gruState_;

    /**
     * Initialize delegate (XNNPACK or CoreML)
     */
    bool initializeDelegate(int num_threads);
    void releaseResources();
    bool cacheTensorIndices();
};

} // namespace ddsp
