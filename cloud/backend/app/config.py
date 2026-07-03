import os
class Settings:
    DB_URL = os.getenv("DB_URL", "sqlite:///./smart_speaker.db")
    JWT_SECRET = os.getenv("JWT_SECRET", "dev-secret-change-me")
    JWT_ALGO = "HS256"
    OTA_DIR = os.getenv("OTA_DIR", "./ota_models")
settings = Settings()
