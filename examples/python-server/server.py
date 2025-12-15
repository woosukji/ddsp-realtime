import asyncio
import json
import os
import sys
import time
from array import array

import websockets

# Try to import the native module
# You need to ensure the compiled .so/.pyd file is in the python path
try:
    sys.path.append("lib") 
    import ddsp_python
except ImportError:
    ddsp_python = None

SAMPLE_RATE = 48000
FRAME_MS = 20
FRAME_SAMPLES = int(SAMPLE_RATE * FRAME_MS / 1000)  # 960

# Model path: Check environment variable first, then use default
MODEL_PATH = os.getenv("DDSP_MODEL_PATH")
if not MODEL_PATH:
    # Default: models/ at project root (../../models from this script)
    MODEL_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../models/Violin.tflite"))

async def audio_session(websocket):
    print(f"Client connected from {websocket.remote_address}")
    
    # Initialize State
    # Up to 3-note chord control (f0s)
    current_f0s = (440.0,)
    current_loudness = 0.5
    
    processors = None
    
    if ddsp_python:
        try:
            if not os.path.exists(MODEL_PATH):
                print(f"Error: Model file not found at {MODEL_PATH}")
                await websocket.close(reason="Model not found")
                return

            # 3-voice (triad) chord synthesis: create 3 processors.
            processors = [
                ddsp_python.DDSPProcessor(MODEL_PATH, float(SAMPLE_RATE), FRAME_SAMPLES)
                for _ in range(3)
            ]
            print("DDSP Processors Initialized (3 voices)")
        except Exception as e:
            print(f"Failed to initialize DDSP: {e}")
            await websocket.close(reason=f"DDSP Init Failed: {e}")
            return
    else:
        print("Error: ddsp_python module not found. Please build the native C++ extension.")
        await websocket.close(reason="Server missing native module")
        return

    async def recv_control_loop():
        nonlocal current_f0s, current_loudness
        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    continue
                try:
                    data = json.loads(message)
                    
                    # Protocol (chord): {"f0s": [440.0, 550.0, 660.0], "loudness": 0.5}
                    # Backward compat: {"f0": 440.0, "loudness": 0.5}
                    # Also supports legacy {"type": "gain", "value": 0.5}
                    
                    if "f0s" in data:
                        raw = data.get("f0s", []) or []
                        f0_list = []
                        for v in raw:
                            try:
                                f0 = float(v)
                            except (TypeError, ValueError):
                                continue
                            if f0 > 0.0:
                                f0_list.append(f0)
                            if len(f0_list) >= 3:
                                break
                        current_f0s = tuple(f0_list)
                    elif "f0" in data:
                        # single-note fallback
                        try:
                            f0 = float(data["f0"])
                            current_f0s = (f0,) if f0 > 0.0 else ()
                        except (TypeError, ValueError):
                            pass
                    
                    if "loudness" in data:
                        current_loudness = float(data["loudness"])
                    elif data.get("type") == "gain":
                        current_loudness = float(data.get("value", 0.5))
                        
                except Exception as e:
                    print(f"Failed to parse control message: {e}")
        except websockets.ConnectionClosed:
            pass
        except Exception as e:
            print(f"Control loop error: {e}")

    # Start receiving controls
    recv_task = asyncio.create_task(recv_control_loop())

    try:
        frame_duration = FRAME_MS / 1000.0
        start_time = time.monotonic()
        frame_index = 0
        silence_bytes = b"\x00" * (FRAME_SAMPLES * 2)  # int16 mono
        host_is_big_endian = (sys.byteorder == "big")

        print("Starting audio stream...")
        while True:
            # 1. Sync to real-time clock to maintain stable streaming rate
            target_time = start_time + frame_index * frame_duration
            now = time.monotonic()
            sleep_time = target_time - now
            
            if sleep_time > 0:
                await asyncio.sleep(sleep_time)

            # 2. Generate Audio via DDSP C++ Core (up to 3 voices)
            # process() returns raw int16 bytes (mono, little-endian)
            f0s = current_f0s  # snapshot
            voice_count = min(len(f0s), 3)

            if voice_count <= 0:
                audio_bytes = silence_bytes
            elif voice_count == 1:
                audio_bytes = processors[0].process(float(f0s[0]), float(current_loudness))
            else:
                # Mix: scale each voice by 1/voice_count, then sum (i.e., average)
                voice_samples = []
                frame_len = None
                for i in range(voice_count):
                    b = processors[i].process(float(f0s[i]), float(current_loudness))
                    a = array("h")
                    a.frombytes(b)
                    if host_is_big_endian:
                        a.byteswap()
                    if frame_len is None:
                        frame_len = len(a)
                    else:
                        frame_len = min(frame_len, len(a))
                    voice_samples.append(a)

                if not voice_samples or frame_len is None or frame_len <= 0:
                    audio_bytes = silence_bytes
                else:
                    inv = 1.0 / float(voice_count)
                    mixed = array("h", [0] * frame_len)
                    for j in range(frame_len):
                        acc = 0.0
                        for i in range(voice_count):
                            acc += voice_samples[i][j] * inv
                        # round + clip to int16
                        x = int(round(acc))
                        if x > 32767:
                            x = 32767
                        elif x < -32768:
                            x = -32768
                        mixed[j] = x
                    if host_is_big_endian:
                        mixed.byteswap()
                    audio_bytes = mixed.tobytes()
            
            # 3. Send to client
            await websocket.send(audio_bytes)

            frame_index += 1

    except websockets.ConnectionClosed:
        print("Client disconnected")
    except Exception as e:
        print(f"Streaming error: {e}")
    finally:
        recv_task.cancel()

async def main():
    if ddsp_python is None:
        print("WARNING: ddsp_python module could not be imported.")
        print("Ensure you have built the native extension and it is in your PYTHONPATH.")
    
    server = await websockets.serve(audio_session, "0.0.0.0", 8765)
    print(f"DDSP Server listening on ws://0.0.0.0:8765")
    print(f"Target Model: {MODEL_PATH}")
    
    await asyncio.Future()  # run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped.")
