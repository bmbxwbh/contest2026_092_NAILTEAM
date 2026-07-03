import hashlib, os, shutil
from sqlalchemy.orm import Session
from ..models import OtaVersion
from ..config import settings

def save_model(db: Session, model_type: str, version: str, data: bytes) -> OtaVersion:
    os.makedirs(settings.OTA_DIR, exist_ok=True)
    path = os.path.join(settings.OTA_DIR, f"{model_type}_{version}.tflite")
    with open(path, "wb") as f:
        f.write(data)
    checksum = hashlib.sha256(data).hexdigest()
    ver = OtaVersion(model_type=model_type, version=version, path=path,
                     size=len(data), checksum=checksum)
    db.add(ver); db.commit(); db.refresh(ver)
    return ver

def latest_version(db: Session, model_type: str) -> OtaVersion | None:
    return db.query(OtaVersion).filter_by(model_type=model_type).order_by(OtaVersion.created_at.desc()).first()
