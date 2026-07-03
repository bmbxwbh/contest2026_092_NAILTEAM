from pydantic import BaseModel
from datetime import datetime

class DeviceRegister(BaseModel):
    device_id: str
    name: str | None = None

class SceneReport(BaseModel):
    device_id: str
    scene: str
    confidence: float

class OtaCheck(BaseModel):
    device_id: str
    model_type: str
    current_version: str | None = None
