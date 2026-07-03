from datetime import datetime, timedelta
from jose import jwt
from ..config import settings

def create_device_token(device_id: str) -> str:
    payload = {"sub": device_id, "exp": datetime.utcnow() + timedelta(days=365)}
    return jwt.encode(payload, settings.JWT_SECRET, algorithm=settings.JWT_ALGO)

def verify_token(token: str) -> str | None:
    try:
        payload = jwt.decode(token, settings.JWT_SECRET, algorithms=[settings.JWT_ALGO])
        return payload.get("sub")
    except Exception:
        return None
