from fastapi.testclient import TestClient
from app.main import app
client = TestClient(app)

def test_register_and_report():
    r = client.post("/api/devices/register", json={"device_id":"dev-001","name":"音箱1"})
    assert r.status_code == 200
    token = r.json()["token"]
    r2 = client.post("/api/scenes/report",
                     json={"device_id":"dev-001","scene":"烹饪","confidence":0.92},
                     headers={"Authorization": f"Bearer {token}"})
    assert r2.status_code == 200
    r3 = client.get("/api/scenes/list", params={"device_id":"dev-001"})
    assert r3.status_code == 200
    assert r3.json()[0]["scene"] == "烹饪"
