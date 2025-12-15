import asyncio
import json

import websockets

# 서버 맥북의 IP로 바꿔 주세요
SERVER_IP = "10.72.0.169"   # 예시
SERVER_PORT = 8765

# secret 핸드쉐이크를 추가했다면 여기에 적기
USE_SECRET = False
SECRET = "my_simple_secret"   # 서버랑 맞춰야 함


async def main():
    uri = f"ws://{SERVER_IP}:{SERVER_PORT}"
    print(f"Connecting to {uri} ...")

    try:
        async with websockets.connect(uri) as websocket:
            print("✅ Connected to server")

            # (옵션) secret 핸드쉐이크 사용 중이라면 먼저 hello 전송
            if USE_SECRET:
                hello = {
                    "type": "hello",
                    "secret": SECRET,
                }
                hello_str = json.dumps(hello)
                await websocket.send(hello_str)
                print(f"→ sent hello: {hello_str}")

            # 간단한 gain 메시지 전송
            gain_msg = {
                "type": "gain",
                "value": 0.5,
            }
            gain_str = json.dumps(gain_msg)
            await websocket.send(gain_str)
            print(f"→ sent gain: {gain_str}")

            # 서버에서 오는 첫 메시지(보통 오디오 프레임)를 받아본다
            print("⌛ waiting for first message from server...")
            msg = await websocket.recv()

            if isinstance(msg, bytes):
                print(f"✅ received binary data ({len(msg)} bytes)")
                # 앞 16바이트 정도만 hex로 찍어보기
                print("   first 16 bytes (hex):", msg[:16].hex())
            else:
                print(f"✅ received text message: {msg}")

    except Exception as e:
        print("❌ connection or communication error:", e)


if __name__ == "__main__":
    asyncio.run(main())
