"""上传 tflite 到云端 OTA(开发期手动/CI 自动)."""
import httpx, sys, os

def upload(server, model_type, version, path):
    with open(path, "rb") as f:
        data = f.read()
    r = httpx.post(f"{server}/api/ota/upload", params={"model_type":model_type,"version":version}, content=data)
    print(r.status_code, r.text)

if __name__ == "__main__":
    upload("http://localhost:8000", "scene_cnn", "0.0.1", "data/scene_cnn.tflite")
