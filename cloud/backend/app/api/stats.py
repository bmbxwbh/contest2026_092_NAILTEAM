from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session
from sqlalchemy import func
from ..database import get_db
from ..models import SceneRecord

router = APIRouter()

@router.get("/scene_distribution")
def scene_distribution(device_id: str = Query(...), db: Session = Depends(get_db)):
    rows = db.query(SceneRecord.scene, func.count(SceneRecord.id)).filter_by(device_id=device_id).group_by(SceneRecord.scene).all()
    return [{"scene": s, "count": c} for s, c in rows]

@router.get("/hourly")
def hourly(device_id: str = Query(...), db: Session = Depends(get_db)):
    rows = db.query(func.strftime("%H", SceneRecord.ts).label("h"), func.count(SceneRecord.id)).filter_by(device_id=device_id).group_by("h").all()
    return [{"hour": int(h), "count": c} for h, c in rows]
