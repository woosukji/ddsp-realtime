# DDSP Models

Pre-trained TensorFlow Lite models for DDSP synthesis.

## Available Models

### Violin.tflite (7.5 MB)

- **Instrument**: Violin
- **Sample Rate**: 48kHz
- **Training Data**: NSynth dataset
- **Architecture**: Encoder-decoder with harmonic + noise synthesis
- **Latency**: ~5ms inference time (M1 Mac with CoreML)

**Usage**:
```cpp
pipeline.loadModel("models/Violin.tflite");
```

## Model Format

All models are TensorFlow Lite (`.tflite`) format with the following specifications:

### Input

| Name | Shape | Type | Description |
|------|-------|------|-------------|
| `f0_hz` | `[1, T]` | float32 | Fundamental frequency in Hz |
| `loudness` | `[1, T]` | float32 | Loudness (normalized 0-1) |

Where `T` is the number of time frames (typically 50 for 1 second @ 48kHz).

### Output

| Name | Shape | Type | Description |
|------|-------|------|-------------|
| `harmonic_amps` | `[1, T, 64]` | float32 | Harmonic amplitudes (64 harmonics) |
| `harmonic_dist` | `[1, T, 64]` | float32 | Harmonic distribution |
| `noise_mags` | `[1, T, 65]` | float32 | Noise filter magnitudes |

The synthesizer then generates audio from these control signals.

## Training Your Own Models

To train custom models using the official DDSP library:

### 1. Prepare Dataset

Organize audio files:
```
dataset/
└── audio/
    ├── instrument1_note1.wav
    ├── instrument1_note2.wav
    └── ...
```

Requirements:
- 48kHz sample rate (mono)
- 4-second clips recommended
- Consistent instrument/timbre

### 2. Extract Features

```python
import ddsp
import ddsp.training

# Extract f0 and loudness
audio_files = glob.glob("dataset/audio/*.wav")
ddsp.training.data.prepare_tfrecord(
    audio_files,
    output_tfrecord="dataset/train.tfrecord",
    sample_rate=48000,
    frame_rate=250  # 250 Hz = 4ms frames
)
```

### 3. Train Model

```python
# train.py
import ddsp
import tensorflow as tf

# Configure model
model = ddsp.training.models.Autoencoder(
    preprocessor=ddsp.training.preprocessing.DefaultPreprocessor(),
    encoder=ddsp.training.encoders.MfccTimeDistributedRnnEncoder(),
    decoder=ddsp.training.decoders.RnnFcDecoder(),
    processor_group=ddsp.training.processors.ProcessorGroup(
        dag=[
            ('harmonic', ddsp.synths.Harmonic()),
            ('noise', ddsp.synths.FilteredNoise()),
            ('add', ddsp.processors.Add())
        ]
    )
)

# Train
trainer = ddsp.training.Trainer(model, strategy=...)
trainer.fit(dataset, epochs=10000)
```

### 4. Convert to TFLite

```python
# convert.py
import tensorflow as tf

# Load saved model
model = tf.saved_model.load("checkpoints/model")

# Convert to TFLite
converter = tf.lite.TFLiteConverter.from_saved_model("checkpoints/model")
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.target_spec.supported_ops = [
    tf.lite.OpsSet.TFLITE_BUILTINS,
    tf.lite.OpsSet.SELECT_TF_OPS
]

tflite_model = converter.convert()

# Save
with open("MyInstrument.tflite", "wb") as f:
    f.write(tflite_model)
```

### 5. Test in DDSP Realtime

```cpp
pipeline.loadModel("MyInstrument.tflite");
pipeline.setF0Hz(440.0);
pipeline.setLoudnessNorm(0.8);
```

## Model Optimization

### Size Reduction

Use quantization to reduce model size:

```python
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset_gen
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_quant_model = converter.convert()
```

Typical size reduction: 7.5MB → 2MB

### CoreML Compatibility

Ensure CoreML compatibility (Apple platforms):

```python
converter.target_spec.supported_ops = [
    tf.lite.OpsSet.TFLITE_BUILTINS,
    tf.lite.OpsSet.SELECT_TF_OPS
]

# Avoid unsupported ops:
# - Custom ops
# - Dynamic shapes
# - Sparse tensors
```

## License

Models in this directory are derived from the NSynth dataset and are subject to:

- **NSynth Dataset**: [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/)
- **DDSP Library**: [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)

When using these models in your application, please provide attribution:

```
Models trained using Google's DDSP library and NSynth dataset.
https://github.com/magenta/ddsp
https://magenta.tensorflow.org/datasets/nsynth
```

## Adding More Models

To add models to this repository:

1. Train and convert to TFLite format
2. Add to `models/` directory
3. Update this README with model specifications
4. Include license information
5. Test with DDSP Realtime

## Model Repository

For more pre-trained models, see:

- [DDSP Official Models](https://github.com/magenta/ddsp/tree/main/ddsp/training/models)
- [NSynth Dataset](https://magenta.tensorflow.org/datasets/nsynth)
- [Community Models](https://github.com/topics/ddsp) (user-contributed)

## References

- [DDSP Paper](https://openreview.net/forum?id=B1x1ma4tDr) - Original DDSP research
- [NSynth Paper](https://arxiv.org/abs/1704.01279) - NSynth dataset
- [TFLite Guide](https://www.tensorflow.org/lite/guide) - TensorFlow Lite documentation
