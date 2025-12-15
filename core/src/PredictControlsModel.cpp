#include "../include/PredictControlsModel.h"

// TFLite C API
#include "tensorflow/lite/core/c/c_api.h"
#include "tensorflow/lite/core/c/c_api_experimental.h"

// Delegates
#include "tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#if TARGET_OS_IOS || TARGET_OS_VISION
#include "tensorflow/lite/delegates/coreml/coreml_delegate.h"
#endif
#endif

#include <cmath>
#include <iostream>
#include <string>

namespace ddsp {

namespace {

template <typename T>
bool CopyToTensor(TfLiteTensor* tensor, const T* src, size_t count) {
    if (!tensor) {
        return false;
    }
    return TfLiteTensorCopyFromBuffer(tensor, src, sizeof(T) * count) == kTfLiteOk;
}

template <typename T>
bool CopyFromTensor(const TfLiteTensor* tensor, T* dst, size_t count) {
    if (!tensor) {
        return false;
    }
    return TfLiteTensorCopyToBuffer(tensor, dst, sizeof(T) * count) == kTfLiteOk;
}

} // namespace

PredictControlsModel::PredictControlsModel()
    : model_loaded_(false)
    , model_(nullptr)
    , interpreter_options_(nullptr)
    , interpreter_(nullptr)
    , delegate_(nullptr)
    , delegate_type_(DelegateType::None)
{
    gruState_.fill(0.0f);
}

PredictControlsModel::~PredictControlsModel() {
    releaseResources();
}

void PredictControlsModel::releaseResources() {
    model_loaded_ = false;

    if (interpreter_) {
        TfLiteInterpreterDelete(interpreter_);
        interpreter_ = nullptr;
    }

    if (delegate_) {
#ifdef __APPLE__
#if TARGET_OS_IOS || TARGET_OS_VISION
        if (delegate_type_ == DelegateType::CoreML) {
            TfLiteCoreMlDelegateDelete(delegate_);
        } else
#endif
#endif
        if (delegate_type_ == DelegateType::XNNPACK) {
            TfLiteXNNPackDelegateDelete(delegate_);
        }

        delegate_ = nullptr;
        delegate_type_ = DelegateType::None;
    }

    if (interpreter_options_) {
        TfLiteInterpreterOptionsDelete(interpreter_options_);
        interpreter_options_ = nullptr;
    }

    if (model_) {
        TfLiteModelDelete(model_);
        model_ = nullptr;
    }

    input_indices_.clear();
    output_indices_.clear();
}

bool PredictControlsModel::loadModel(const std::string& model_path, int num_threads) {
    releaseResources();

    model_ = TfLiteModelCreateFromFile(model_path.c_str());
    if (!model_) {
        std::cerr << "Failed to load model from: " << model_path << std::endl;
        return false;
    }

    interpreter_options_ = TfLiteInterpreterOptionsCreate();
    if (!interpreter_options_) {
        std::cerr << "Failed to create interpreter options" << std::endl;
        releaseResources();
        return false;
    }

    TfLiteInterpreterOptionsSetNumThreads(interpreter_options_, num_threads);
    if (!initializeDelegate(num_threads)) {
        std::cerr << "Warning: Failed to initialize delegate, falling back to CPU" << std::endl;
    }

    interpreter_ = TfLiteInterpreterCreate(model_, interpreter_options_);
    if (!interpreter_) {
        std::cerr << "Failed to create interpreter" << std::endl;
        releaseResources();
        return false;
    }

    if (TfLiteInterpreterAllocateTensors(interpreter_) != kTfLiteOk) {
        std::cerr << "Failed to allocate tensors" << std::endl;
        releaseResources();
        return false;
    }

    if (!cacheTensorIndices()) {
        std::cerr << "Failed to cache tensor indices for model" << std::endl;
        releaseResources();
        return false;
    }

    model_loaded_ = true;
    reset();

    std::cout << "Model loaded successfully (" << input_indices_.size()
              << " inputs, " << output_indices_.size() << " outputs)" << std::endl;
    return true;
}

bool PredictControlsModel::initializeDelegate(int num_threads) {
#ifdef __APPLE__
#if TARGET_OS_IOS || TARGET_OS_VISION
    TfLiteCoreMlDelegateOptions coreml_options = {};
    coreml_options.enabled_devices = TfLiteCoreMlDelegateAllDevices;
    coreml_options.coreml_version = 3;
    coreml_options.max_delegated_partitions = 0;
    coreml_options.min_nodes_per_partition = 2;

    delegate_ = TfLiteCoreMlDelegateCreate(&coreml_options);
    if (delegate_) {
        TfLiteInterpreterOptionsAddDelegate(
            interpreter_options_,
            reinterpret_cast<TfLiteOpaqueDelegate*>(delegate_));
        delegate_type_ = DelegateType::CoreML;
        std::cout << "CoreML delegate configured" << std::endl;
        return true;
    }
#endif
#endif

    TfLiteXNNPackDelegateOptions xnnpack_options = TfLiteXNNPackDelegateOptionsDefault();
    xnnpack_options.num_threads = num_threads;
    delegate_ = TfLiteXNNPackDelegateCreate(&xnnpack_options);
    if (delegate_) {
        TfLiteInterpreterOptionsAddDelegate(
            interpreter_options_,
            reinterpret_cast<TfLiteOpaqueDelegate*>(delegate_));
        delegate_type_ = DelegateType::XNNPACK;
        std::cout << "XNNPACK delegate configured" << std::endl;
        return true;
    }

    delegate_type_ = DelegateType::None;
    return false;
}

bool PredictControlsModel::cacheTensorIndices() {
    if (!interpreter_) {
        return false;
    }

    input_indices_.clear();
    output_indices_.clear();

    const int input_count = TfLiteInterpreterGetInputTensorCount(interpreter_);
    for (int i = 0; i < input_count; ++i) {
        TfLiteTensor* tensor = TfLiteInterpreterGetInputTensor(interpreter_, i);
        if (!tensor) {
            continue;
        }
        const char* name = TfLiteTensorName(tensor);
        if (name) {
            input_indices_[name] = i;
        }
    }

    const int output_count = TfLiteInterpreterGetOutputTensorCount(interpreter_);
    for (int i = 0; i < output_count; ++i) {
        const TfLiteTensor* tensor = TfLiteInterpreterGetOutputTensor(interpreter_, i);
        if (!tensor) {
            continue;
        }
        const char* name = TfLiteTensorName(tensor);
        if (name) {
            output_indices_[name] = i;
        }
    }

    auto ensure = [&](std::string_view name, const std::unordered_map<std::string, int>& map) {
        return map.find(std::string(name)) != map.end();
    };

    bool ok = ensure(kInputTensorName_F0, input_indices_) &&
              ensure(kInputTensorName_Loudness, input_indices_) &&
              ensure(kInputTensorName_State, input_indices_) &&
              ensure(kOutputTensorName_Amplitude, output_indices_) &&
              ensure(kOutputTensorName_Harmonics, output_indices_) &&
              ensure(kOutputTensorName_NoiseAmps, output_indices_) &&
              ensure(kOutputTensorName_State, output_indices_);

    if (!ok) {
        std::cerr << "Failed to locate all required tensors in the model." << std::endl;
    }

    return ok;
}

bool PredictControlsModel::call(const AudioFeatures& input, SynthesisControls& output) {
    if (!model_loaded_ || !interpreter_) {
        return false;
    }

    auto getInputTensor = [&](std::string_view name) -> TfLiteTensor* {
        auto it = input_indices_.find(std::string(name));
        if (it == input_indices_.end()) {
            return nullptr;
        }
        return TfLiteInterpreterGetInputTensor(interpreter_, it->second);
    };

    TfLiteTensor* f0_tensor = getInputTensor(kInputTensorName_F0);
    TfLiteTensor* loudness_tensor = getInputTensor(kInputTensorName_Loudness);
    TfLiteTensor* state_tensor = getInputTensor(kInputTensorName_State);

    if (!CopyToTensor(f0_tensor, &input.f0_norm, 1) ||
        !CopyToTensor(loudness_tensor, &input.loudness_norm, 1) ||
        !CopyToTensor(state_tensor, gruState_.data(), gruState_.size())) {
        std::cerr << "Failed to populate input tensors" << std::endl;
        return false;
    }

    if (TfLiteInterpreterInvoke(interpreter_) != kTfLiteOk) {
        std::cerr << "Inference failed" << std::endl;
        return false;
    }

    auto getOutputTensor = [&](std::string_view name) -> const TfLiteTensor* {
        auto it = output_indices_.find(std::string(name));
        if (it == output_indices_.end()) {
            return nullptr;
        }
        return TfLiteInterpreterGetOutputTensor(interpreter_, it->second);
    };

    const TfLiteTensor* amplitude_tensor = getOutputTensor(kOutputTensorName_Amplitude);
    const TfLiteTensor* harmonics_tensor = getOutputTensor(kOutputTensorName_Harmonics);
    const TfLiteTensor* noise_tensor = getOutputTensor(kOutputTensorName_NoiseAmps);
    const TfLiteTensor* gru_tensor = getOutputTensor(kOutputTensorName_State);

    if (!CopyFromTensor(amplitude_tensor, &output.amplitude, 1) ||
        !CopyFromTensor(harmonics_tensor, output.harmonics.data(), output.harmonics.size()) ||
        !CopyFromTensor(noise_tensor, output.noiseAmps.data(), output.noiseAmps.size()) ||
        !CopyFromTensor(gru_tensor, gruState_.data(), gruState_.size())) {
        std::cerr << "Failed to read output tensors" << std::endl;
        return false;
    }

    for (float& harmonic : output.harmonics) {
        if (std::isnan(harmonic)) {
            harmonic = 0.0f;
            output.amplitude = 0.0f;
        }
    }

    output.f0_hz = input.f0_hz;
    return true;
}

void PredictControlsModel::reset() {
    gruState_.fill(0.0f);
}

} // namespace ddsp
