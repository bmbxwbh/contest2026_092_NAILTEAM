from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session
from ..database import get_db
from ..models import SceneRecord
from ..schemas import SceneReport

router = APIRouter()

@router.post("/report")
def report(payload: SceneReport, db: Session = Depends(get_db)):
    rec = SceneRecord(device_id=payload.device_id, scene=payload.scene, confidence=payload.confidence)
    db.add(rec); db.commit()
    return {"status": "ok"}

@router.get("/list")
def list_scenes(device_id: str = Query(...), limit: int = 100, db: Session = Depends(get_db)):
    rows = db.query(SceneRecord).filter_by(device_id=device_id).order_by(SceneRecord.ts.desc()).limit(limit).all()
    return [{"scene": r.scene, "confidence": r.confidence, "ts": r.ts.isoformat()} for r in rows]
