from datetime import datetime
from sqlalchemy import Column, Integer, String, Float, DateTime, ForeignKey, Text
from .database import Base

class Device(Base):
    __tablename__ = "devices"
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), unique=True, index=True)
    name = Column(String(64))
    token = Column(String(128))
    created_at = Column(DateTime, default=datetime.utcnow)

class SceneRecord(Base):
    __tablename__ = "scene_records"
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), index=True)
    scene = Column(String(32))
    confidence = Column(Float)
    ts = Column(DateTime, default=datetime.utcnow)

class OtaVersion(Base):
    __tablename__ = "ota_versions"
    id = Column(Integer, primary_key=True, index=True)
    model_type = Column(String(32), index=True)
    version = Column(String(32))
    path = Column(String(256))
    size = Column(Integer)
    checksum = Column(String(64))
    created_at = Column(DateTime, default=datetime.utcnow)
