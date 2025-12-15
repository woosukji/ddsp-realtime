# DDSP Unity Plugin Example

Unity Native Audio Plugin for real-time DDSP synthesis.

## Overview

This example demonstrates how to use `ddsp_core` as a Unity Native Audio Plugin. It provides:

- Real-time neural audio synthesis in Unity
- Native performance (C++ implementation)
- Cross-platform support (macOS, iOS, visionOS)
- Full Unity Mixer integration
- Parameter automation support

## Building

### Prerequisites

- Unity 2020.3 or later
- Xcode (macOS/iOS builds)
- CMake 3.20+
- `ddsp_core` built and available

### Build Steps

#### macOS

```bash
./build.sh
```

Output: `build/AudioPluginDDSP.bundle`

#### iOS

```bash
PLATFORM=ios ./build.sh
```

Output: `build/AudioPluginDDSP.framework`

#### visionOS

```bash
PLATFORM=visionos ./build.sh
```

Output: `build/AudioPluginDDSP.framework`

## Installation in Unity

### 1. Copy Plugin Files

Copy the built plugin to your Unity project:

```
YourUnityProject/
└── Assets/
    └── Plugins/
        ├── macOS/
        │   └── AudioPluginDDSP.bundle
        └── iOS/
            └── AudioPluginDDSP.framework
```

### 2. Configure Plugin Import Settings

Select the plugin in Unity Editor:

1. **Inspector → Platform Settings**
   - **macOS**: Enable "Standalone"
   - **iOS**: Enable "iOS"
2. **Load on startup**: ✅ Checked
3. **CPU**: Any

### 3. Copy C# Scripts

Copy the Unity scripts to your project:

```bash
cp unity/Scripts/*.cs YourUnityProject/Assets/Scripts/
```

## Usage in Unity

### Method 1: Audio Source Component

Attach `DDSPAudioSource` to a GameObject:

```csharp
using UnityEngine;

public class DDSPTest : MonoBehaviour
{
    private DDSPAudioSource ddspSource;

    void Start()
    {
        ddspSource = gameObject.AddComponent<DDSPAudioSource>();
    }

    void Update()
    {
        // Control frequency (Hz)
        float frequency = 440.0f + Mathf.Sin(Time.time) * 100.0f;
        ddspSource.SetF0(frequency);

        // Control loudness (0-1)
        float loudness = (Mathf.Sin(Time.time * 2.0f) + 1.0f) * 0.5f;
        ddspSource.SetLoudness(loudness);

        // Pitch shift (semitones)
        ddspSource.SetPitchShift(0.0f);
    }
}
```

### Method 2: Unity Mixer Effect

1. **Create Audio Mixer**:
   - Right-click in Project → Create → Audio Mixer
2. **Add DDSP Effect**:
   - Select mixer group → Add Effect → DDSP Synth
3. **Configure Parameters**:
   - F0: Fundamental frequency (440-660 Hz)
   - Loudness: Amplitude (0-1)
   - Pitch Shift: Semitones (-24 to +24)
   - Harmonic Gain: Harmonic content (0-2)
   - Noise Gain: Noise content (0-2)
   - Output Gain: Final gain (-60 to +12 dB)

### Method 3: Controller Script

Use `DDSPController` for more control:

```csharp
using UnityEngine;

public class InstrumentPlayer : MonoBehaviour
{
    public DDSPController ddspController;

    void Update()
    {
        // Play note on key press
        if (Input.GetKeyDown(KeyCode.A))
        {
            ddspController.PlayNote(440.0f, 0.8f);  // A4, velocity 0.8
        }

        // Stop note
        if (Input.GetKeyUp(KeyCode.A))
        {
            ddspController.StopNote();
        }

        // Bend pitch
        if (Input.GetKey(KeyCode.LeftShift))
        {
            ddspController.SetPitchBend(2.0f);  // +2 semitones
        }
        else
        {
            ddspController.SetPitchBend(0.0f);
        }
    }
}
```

## Parameters

### Unity Mixer Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **F0** | 440-660 Hz | 440 Hz | Fundamental frequency |
| **Loudness** | 0-1 | 0.5 | Normalized loudness |
| **Pitch Shift** | -24 to +24 st | 0 st | Pitch shift in semitones |
| **Harmonic Gain** | 0-2 | 1.0 | Harmonic synthesis gain |
| **Noise Gain** | 0-2 | 1.0 | Noise synthesis gain |
| **Output Gain** | -60 to +12 dB | 0 dB | Final output gain |

### C# API

```csharp
// Set frequency directly
void SetF0(float hz);

// Set loudness (0-1)
void SetLoudness(float loudness);

// Pitch shift in semitones
void SetPitchShift(float semitones);

// Control harmonic/noise balance
void SetHarmonicGain(float gain);
void SetNoiseGain(float gain);

// Output gain in dB
void SetOutputGain(float db);

// High-level note control
void PlayNote(float frequency, float velocity);
void StopNote();
void SetPitchBend(float semitones);
```

## Models

### Setting Model Path

The plugin loads models from:

1. **Environment variable** (highest priority):
   ```bash
   export DDSP_MODEL_PATH=/path/to/model.tflite
   ```

2. **Default path** (relative to plugin):
   ```
   ../../models/Violin.tflite
   ```

### Bundling Models with Unity

To include models in your Unity build:

1. Create `StreamingAssets/DDSP/` folder
2. Copy `.tflite` files there
3. Update plugin to load from `Application.streamingAssetsPath`

## Performance

### Benchmarks (Apple M1, Unity 2022.3)

- **Latency**: ~10ms (512 samples @ 48kHz)
- **CPU Usage**: ~3% (single voice)
- **Memory**: ~25MB (including Unity overhead)

### Optimization Tips

1. **Buffer Size**: Use 512-1024 samples for best latency/CPU balance
2. **Sample Rate**: 48kHz recommended (model trained at 48kHz)
3. **Voice Count**: Limit to 3-4 simultaneous voices for mobile
4. **CoreML**: Enable on iOS/macOS for 50% CPU reduction

## Troubleshooting

### Plugin Not Found

**Symptom**: Unity console shows "Failed to load plugin"

**Solution**:
1. Verify plugin is in correct `Assets/Plugins/<platform>/` folder
2. Check import settings (Platform Settings → Enable target platform)
3. Restart Unity Editor

### No Audio Output

**Symptom**: Plugin loads but produces silence

**Solution**:
1. Check model path:
   ```csharp
   Debug.Log(System.Environment.GetEnvironmentVariable("DDSP_MODEL_PATH"));
   ```
2. Verify model file exists and is readable
3. Check Unity Audio Settings (Edit → Project Settings → Audio):
   - DSP Buffer Size: "Best latency" or "Good latency"
   - Sample Rate: 48000 Hz

### Crackling Audio

**Symptom**: Audio output has clicks/pops

**Solution**:
1. Increase DSP buffer size (Project Settings → Audio)
2. Reduce `startTimer()` frequency in plugin (default 20ms)
3. Enable CoreML delegate if on iOS/macOS

### iOS Build Fails

**Symptom**: Xcode build error "Framework not found AudioPluginDDSP"

**Solution**:
1. Ensure plugin is built for iOS architecture:
   ```bash
   file build/AudioPluginDDSP.framework/AudioPluginDDSP
   # Should show: arm64
   ```
2. Check Unity build settings:
   - Architecture: ARM64 only
   - Target minimum iOS version: 14.0+

## Example Projects

See `examples/unity-plugin/unity/` for complete example scenes:

- **SimplePlayer**: Basic frequency/loudness control
- **MIDIInput**: Play notes from MIDI controller
- **InteractiveInstrument**: Touch-responsive instrument

## Advanced

### Custom Model Loading

Implement runtime model loading:

```csharp
public class DynamicModelLoader : MonoBehaviour
{
    [SerializeField] private string modelPath;
    private DDSPController controller;

    public void LoadModel(string path)
    {
        // Set environment variable before creating plugin
        System.Environment.SetEnvironmentVariable("DDSP_MODEL_PATH", path);

        // Recreate DDSP instance
        controller = gameObject.AddComponent<DDSPController>();
    }
}
```

### Multiple Instruments

Use multiple plugin instances:

```csharp
public class Orchestra : MonoBehaviour
{
    private DDSPController violin;
    private DDSPController flute;

    void Start()
    {
        // Violin
        System.Environment.SetEnvironmentVariable("DDSP_MODEL_PATH", "models/Violin.tflite");
        violin = gameObject.AddComponent<DDSPController>();

        // Flute
        System.Environment.SetEnvironmentVariable("DDSP_MODEL_PATH", "models/Flute.tflite");
        flute = gameObject.AddComponent<DDSPController>();
    }
}
```

### Wwise Integration

For spatial audio with Wwise, use `DDSPToWwiseInput` component:

```csharp
// Attach to GameObject with AkGameObj
var ddspWwise = gameObject.AddComponent<DDSPToWwiseInput>();
ddspWwise.SetF0(440.0f);
ddspWwise.SetLoudness(0.8f);

// Audio automatically routed to Wwise for 3D positioning
```

## Next Steps

- [Core API Documentation](../../docs/API.md)
- [Build Guide](../../docs/BUILD.md)
- [DDSP Paper](https://arxiv.org/abs/2001.04643)
