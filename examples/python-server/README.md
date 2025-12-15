# DDSP Python Server Example

WebSocket server for real-time DDSP synthesis with Python bindings.

## Overview

This example demonstrates:

- Python bindings for `ddsp_core` using pybind11
- Real-time WebSocket server for remote synthesis
- Multi-voice chord synthesis (up to 3 voices)
- JSON-based control protocol
- Audio streaming over WebSocket

## Building

### Prerequisites

- Python 3.8 or later
- CMake 3.20+
- `ddsp_core` built and available

### Build Steps

```bash
# Install Python dependencies
pip3 install -r requirements.txt

# Build Python bindings
./build.sh
```

Output: `lib/ddsp_python.so` (or `.pyd` on Windows)

### Verify Installation

```bash
python3 -c "import sys; sys.path.append('lib'); import ddsp_python; print('Success!')"
```

## Running the Server

### Start Server

```bash
python3 server.py
```

Output:
```
Starting DDSP WebSocket Server on ws://localhost:8765
Server ready. Waiting for connections...
```

### Configuration

Edit `server.py` to change settings:

```python
SAMPLE_RATE = 48000      # Audio sample rate (Hz)
FRAME_MS = 20            # Frame duration (ms)
HOST = "0.0.0.0"         # Server host (use 0.0.0.0 for external access)
PORT = 8765              # Server port
```

### Model Path

Set via environment variable:
```bash
export DDSP_MODEL_PATH=/path/to/model.tflite
python3 server.py
```

Or edit `server.py`:
```python
MODEL_PATH = "/path/to/your/model.tflite"
```

## Usage

### Client Protocol

The server accepts JSON messages over WebSocket:

```json
{
  "f0s": [440.0, 550.0, 660.0],  // Frequencies (Hz) - up to 3 voices
  "loudness": 0.7                 // Overall loudness (0-1)
}
```

### Python Client Example

```python
import asyncio
import json
import websockets

async def play_note():
    uri = "ws://localhost:8765"
    async with websockets.connect(uri) as websocket:
        # Send control parameters
        message = {
            "f0s": [440.0],  # Single note (A4)
            "loudness": 0.8
        }
        await websocket.send(json.dumps(message))

        # Receive audio data (binary)
        audio_data = await websocket.recv()
        print(f"Received {len(audio_data)} bytes of audio")

asyncio.run(play_note())
```

### JavaScript Client Example

```javascript
const ws = new WebSocket('ws://localhost:8765');

ws.onopen = () => {
    // Send chord (C major)
    const message = {
        f0s: [261.63, 329.63, 392.00],  // C4, E4, G4
        loudness: 0.7
    };
    ws.send(JSON.stringify(message));
};

ws.onmessage = (event) => {
    // Receive audio data as ArrayBuffer
    const audioData = event.data;
    console.log(`Received ${audioData.byteLength} bytes`);
};
```

### cURL Example (Testing)

```bash
# Note: cURL can't handle WebSocket binary data well, use for testing only
curl -i -N \
    -H "Connection: Upgrade" \
    -H "Upgrade: websocket" \
    -H "Sec-WebSocket-Version: 13" \
    -H "Sec-WebSocket-Key: test" \
    http://localhost:8765
```

## Python Bindings API

### DDSPProcessor Class

```python
import sys
sys.path.append('lib')
import ddsp_python

# Create processor
processor = ddsp_python.DDSPProcessor(
    model_path="/path/to/model.tflite",
    sample_rate=48000.0,
    block_size=960
)

# Set control parameters
processor.set_f0(440.0)           # Frequency (Hz)
processor.set_loudness(0.8)        # Loudness (0-1)
processor.set_pitch_shift(2.0)     # Pitch shift (semitones)

# Get audio output
audio_buffer = processor.get_next_block(960)  # Returns list of floats
```

### Full API Reference

```python
class DDSPProcessor:
    def __init__(self, model_path: str, sample_rate: float, block_size: int)

    # Control methods
    def set_f0(self, hz: float) -> None
    def set_loudness(self, loudness: float) -> None  # 0-1
    def set_pitch_shift(self, semitones: float) -> None
    def set_harmonic_gain(self, gain: float) -> None
    def set_noise_gain(self, gain: float) -> None

    # Audio output
    def get_next_block(self, num_samples: int) -> list[float]

    # Reset
    def reset(self) -> None
```

## Advanced Usage

### Multi-voice Synthesis

The server supports up to 3 simultaneous voices (configurable):

```python
# Play chord
message = {
    "f0s": [261.63, 329.63, 392.00],  # C major triad
    "loudness": 0.7
}
```

Each voice uses a separate `DDSPProcessor` instance, and outputs are mixed together.

### MIDI File Rendering

Render MIDI files to audio using `render_midi.py`:

```bash
python3 render_midi.py input.mid output.wav
```

This script:
1. Parses MIDI note events
2. Synthesizes each note using DDSP
3. Writes WAV file

### Latency Testing

Measure round-trip latency:

```bash
python3 latency_test.py
```

Output:
```
Average latency: 12.3ms
Min latency: 8.1ms
Max latency: 18.7ms
```

### Batch Processing

Process multiple control sequences:

```python
import ddsp_python
import numpy as np

# Create processor
processor = ddsp_python.DDSPProcessor("models/Violin.tflite", 48000.0, 960)

# Generate control sequence (1 second)
num_frames = 50  # 50 frames @ 20ms = 1 second
f0_sequence = [440.0 + 50.0 * np.sin(2 * np.pi * i / num_frames)
               for i in range(num_frames)]
loudness_sequence = [0.8] * num_frames

# Render
audio_output = []
for f0, loudness in zip(f0_sequence, loudness_sequence):
    processor.set_f0(f0)
    processor.set_loudness(loudness)
    block = processor.get_next_block(960)
    audio_output.extend(block)

# Save to WAV
import wave
with wave.open('output.wav', 'wb') as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(48000)
    # Convert float32 to int16
    audio_int16 = np.array(audio_output) * 32767.0
    wav.writeframes(audio_int16.astype(np.int16).tobytes())
```

## Performance

### Benchmarks (Apple M1)

- **Latency**: ~15ms (server + network overhead)
- **Throughput**: 2400 frames/sec (20ms frames)
- **CPU Usage**: ~8% (3 voices)
- **Memory**: ~30MB per process

### Optimization Tips

1. **Frame Size**: Use 960 samples (20ms @ 48kHz) for balance
2. **Connection Pool**: Reuse WebSocket connections
3. **Batch Requests**: Send multiple control updates in one message
4. **Compression**: Enable WebSocket compression for control data

## Deployment

### Local Network

```bash
# Allow external connections
python3 server.py --host 0.0.0.0 --port 8765
```

Access from other devices:
```
ws://YOUR_IP:8765
```

### Production Server (systemd)

Create `/etc/systemd/system/ddsp-server.service`:

```ini
[Unit]
Description=DDSP Realtime Server
After=network.target

[Service]
Type=simple
User=ddsp
WorkingDirectory=/opt/ddsp-realtime/examples/python-server
Environment="DDSP_MODEL_PATH=/opt/ddsp-realtime/models/Violin.tflite"
Environment="PYTHONPATH=/opt/ddsp-realtime/examples/python-server/lib"
ExecStart=/usr/bin/python3 server.py
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable ddsp-server
sudo systemctl start ddsp-server
sudo systemctl status ddsp-server
```

### Docker

```dockerfile
FROM python:3.11-slim

WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy built bindings
COPY lib/ddsp_python.so lib/
COPY server.py .

# Copy models
COPY ../../models/ /models/

ENV PYTHONPATH=/app/lib
ENV DDSP_MODEL_PATH=/models/Violin.tflite

EXPOSE 8765

CMD ["python3", "server.py"]
```

Build and run:
```bash
docker build -t ddsp-server .
docker run -p 8765:8765 ddsp-server
```

## Troubleshooting

### Import Error

**Symptom**: `ModuleNotFoundError: No module named 'ddsp_python'`

**Solution**:
```bash
# Ensure module is built
ls -la lib/ddsp_python.so

# Add to Python path
export PYTHONPATH=/path/to/examples/python-server/lib:$PYTHONPATH
```

### Connection Refused

**Symptom**: Client can't connect to WebSocket

**Solution**:
1. Check server is running: `netstat -an | grep 8765`
2. Check firewall: `sudo ufw allow 8765/tcp`
3. Bind to all interfaces: `HOST = "0.0.0.0"`

### Audio Glitches

**Symptom**: Clicks/pops in audio output

**Solution**:
1. Increase frame size: `FRAME_SAMPLES = 1920` (40ms)
2. Reduce voice count (modify server.py)
3. Use faster hardware or enable CoreML

### Model Not Found

**Symptom**: `Error: Model file not found`

**Solution**:
```bash
# Check path
echo $DDSP_MODEL_PATH

# Use absolute path
export DDSP_MODEL_PATH=/absolute/path/to/model.tflite
```

## Example Applications

### Real-time Instrument

Control synthesis from MIDI keyboard:

```python
import mido
import asyncio
import websockets
import json

async def midi_player():
    uri = "ws://localhost:8765"
    async with websockets.connect(uri) as websocket:
        # Open MIDI input
        with mido.open_input() as inport:
            for msg in inport:
                if msg.type == 'note_on':
                    # Convert MIDI note to frequency
                    f0 = 440.0 * 2**((msg.note - 69) / 12.0)
                    loudness = msg.velocity / 127.0

                    # Send to server
                    await websocket.send(json.dumps({
                        "f0s": [f0],
                        "loudness": loudness
                    }))

asyncio.run(midi_player())
```

### Audio Streaming

Stream synthesized audio to browser:

```python
# server_streaming.py
import asyncio
import websockets
import ddsp_python

async def audio_stream(websocket):
    processor = ddsp_python.DDSPProcessor("models/Violin.tflite", 48000, 960)

    while True:
        # Receive control
        msg = await websocket.recv()
        params = json.loads(msg)

        processor.set_f0(params['f0s'][0])
        processor.set_loudness(params['loudness'])

        # Send audio continuously
        audio = processor.get_next_block(960)
        await websocket.send(audio.tobytes())

asyncio.run(websockets.serve(audio_stream, "localhost", 8765))
```

## Next Steps

- [Core API Documentation](../../docs/API.md)
- [Build Guide](../../docs/BUILD.md)
- [Python Bindings Source](bindings/bindings.cpp)
