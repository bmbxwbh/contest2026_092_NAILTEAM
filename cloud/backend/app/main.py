from fastapi import FastAPI
from .database import Base, engine
from .api import devices, scenes, ota, stats

Base.metadata.create_all(bind=engine)
app = FastAPI(title="Smart Speaker Cloud")
app.include_router(devices.router, prefix="/api/devices", tags=["devices"])
app.include_router(scenes.router, prefix="/api/scenes", tags=["scenes"])
app.include_router(ota.router, prefix="/api/ota", tags=["ota"])
app.include_router(stats.router, prefix="/api/stats", tags=["stats"])

@app.get("/health")
def health(): return {"status": "ok"}
