from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from ..database import get_db
from ..models import Device
from ..schemas import DeviceRegister
from ..core.security import create_device_token

router = APIRouter()

@router.post("/register")
def register(payload: DeviceRegister, db: Session = Depends(get_db)):
    dev = db.query(Device).filter_by(device_id=payload.device_id).first()
    if not dev:
        dev = Device(device_id=payload.device_id, name=payload.name or payload.device_id)
        db.add(dev); db.commit(); db.refresh(dev)
    token = create_device_token(dev.device_id)
    dev.token = token; db.commit()
    return {"device_id": dev.device_id, "token": token}
