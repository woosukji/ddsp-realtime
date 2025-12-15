#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "InferencePipeline.h"
#include "MidiInputProcessor.h"
#include "DDSPTypes.h"
#include <cmath>

namespace py = pybind11;

class DDSPProcessor {
public:
    DDSPProcessor(const std::string& model_path, double sample_rate, int block_size) {
        pipeline = std::make_unique<ddsp::InferencePipeline>();
        pipeline->prepareToPlay(sample_rate, block_size);
        
        if (!pipeline->loadModel(model_path)) {
            throw std::runtime_error("Failed to load model: " + model_path);
        }
        
        this->block_size = block_size;
        temp_buffer.resize(block_size);

        // Initialize MIDI Input Processor
        midi_processor = std::make_unique<ddsp::MidiInputProcessor>();
        // Calculate user hop size: sample_rate * kModelHopSize / kModelSampleRate_Hz
        int user_hop_size = static_cast<int>(sample_rate * ddsp::kModelHopSize / ddsp::kModelSampleRate_Hz);
        midi_processor->prepareToPlay(sample_rate, user_hop_size);
    }

    // Direct Parameter Control (Synth Mode)
    py::bytes process(float f0, float loudness) {
        pipeline->setF0Hz(f0);
        pipeline->setLoudnessNorm(loudness);
        return render();
    }

    // MIDI Input Control
    // midi_messages: list of (status, byte1, byte2) tuples
    py::bytes process_midi(const std::vector<std::vector<int>>& midi_messages) {
        juce::MidiBuffer midi_buffer;
        
        // Add messages to JUCE buffer
        // Assuming all messages happen at sample 0 for this block
        for (const auto& msg : midi_messages) {
            if (msg.size() < 1) continue;
            
            int status = msg[0];
            int data1 = msg.size() > 1 ? msg[1] : 0;
            int data2 = msg.size() > 2 ? msg[2] : 0;
            
            auto juce_msg = juce::MidiMessage(status, data1, data2);
            midi_buffer.addEvent(juce_msg, 0);
        }

        // 1. Process MIDI to update ADSR / internal state
        midi_processor->processMidiBuffer(midi_buffer);

        // 2. Get current F0/Loudness from MIDI processor
        auto features = midi_processor->getCurrentPredictControlsInput();

        // 3. Set pipeline parameters
        pipeline->setF0Hz(features.f0_hz);
        pipeline->setLoudnessNorm(features.loudness_norm);

        // 4. Render audio
        return render();
    }

    void reset() {
        pipeline->reset();
        if (midi_processor) {
            midi_processor->reset();
        }
    }

private:
    std::unique_ptr<ddsp::InferencePipeline> pipeline;
    std::unique_ptr<ddsp::MidiInputProcessor> midi_processor;
    std::vector<float> temp_buffer;
    int block_size;

    py::bytes render() {
        // Loop until we have enough data
        // Note: getNumReadySamples is a helper we added to InferencePipeline
        // We might need to trigger render multiple times if block_size > hop_size
        while (pipeline->getNumReadySamples() < block_size) {
            pipeline->triggerRender();
        }
        
        // Read data
        pipeline->getNextBlock(temp_buffer.data(), block_size);
        
        // Convert to int16 for transmission/saving
        std::string output_bytes;
        output_bytes.resize(block_size * sizeof(int16_t));
        int16_t* out_ptr = reinterpret_cast<int16_t*>(&output_bytes[0]);
        
        for(int i=0; i<block_size; ++i) {
            float s = temp_buffer[i];
            // Clip
            if (s > 1.0f) s = 1.0f;
            if (s < -1.0f) s = -1.0f;
            out_ptr[i] = static_cast<int16_t>(s * 32767.0f);
        }
        
        return py::bytes(output_bytes);
    }
};

PYBIND11_MODULE(ddsp_python, m) {
    py::class_<DDSPProcessor>(m, "DDSPProcessor")
        .def(py::init<const std::string&, double, int>())
        .def("process", &DDSPProcessor::process)
        .def("process_midi", &DDSPProcessor::process_midi)
        .def("reset", &DDSPProcessor::reset);
}
