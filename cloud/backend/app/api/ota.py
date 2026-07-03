from fastapi import APIRouter, Depends, HTTPException
from fastapi.responses import FileResponse
from sqlalchemy.orm import Session
from ..database import get_db
from ..models import OtaVersion
from ..schemas import OtaCheck
from ..services.ota_manager import latest_version

router = APIRouter()

@router.post("/check")
def check(payload: OtaCheck, db: Session = Depends(get_db)):
    ver = latest_version(db, payload.model_type)
    if not ver:
        raise HTTPException(404, "no model")
    if ver.version == payload.current_version:
        return {"update": False, "version": ver.version}
    return {"update": True, "version": ver.version, "size": ver.size, "checksum": ver.checksum}

@router.get("/download")
def download(model_type: str, version: str, db: Session = Depends(get_db)):
    ver = db.query(OtaVersion).filter_by(model_type=model_type, version=version).first()
    if not ver:
        raise HTTPException(404, "version not found")
    return FileResponse(ver.path, media_type="application/octet-stream",
                        filename=f"{model_type}_{version}.tflite")
