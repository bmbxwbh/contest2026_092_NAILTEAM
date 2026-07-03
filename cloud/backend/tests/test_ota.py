from fastapi.testclient import TestClient
from app.main import app
client = TestClient(app)

def test_ota_flow():
    fake_model = b"\x1a\x0cTFL3..."
    from app.database import SessionLocal
    from app.services.ota_manager import save_model
    db = SessionLocal()
    save_model(db, "scene_cnn", "0.0.1", fake_model)
    db.close()
    r = client.post("/api/ota/check", json={"device_id":"d1","model_type":"scene_cnn","current_version":None})
    assert r.status_code == 200 and r.json()["update"] is True
    r2 = client.get("/api/ota/download", params={"model_type":"scene_cnn","version":"0.0.1"})
    assert r2.status_code == 200
