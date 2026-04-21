# Project  : HelpDesk
# File     : routes/voice.py
# Purpose  : Voice transcription route (/api/voice/transcribe) via faster-whisper
# Depends  : faster-whisper (optional install)

import logging
import tempfile
from pathlib import Path

from fastapi import APIRouter, HTTPException, Request

router = APIRouter(prefix="/api/voice")

_MAX_VOICE_BYTES = 2 * 1024 * 1024   # 2 MB — 3 s @ 16 kHz 16-bit is only ~96 KB

# Lazily loaded so startup is fast even if faster-whisper isn't installed.
_whisper_model = None


def _get_whisper_model():
    global _whisper_model
    if _whisper_model is None:
        try:
            from faster_whisper import WhisperModel
        except ImportError:
            raise HTTPException(501,
                "faster-whisper is not installed. Run: pip install faster-whisper")
        logging.info("[Voice] Loading Whisper 'small' model (first call only)...")
        _whisper_model = WhisperModel("small", device="cpu", compute_type="int8")
        logging.info("[Voice] Whisper model ready.")
    return _whisper_model


@router.post("/transcribe")
async def voice_transcribe(request: Request):
    """Receives a raw WAV body (Content-Type: audio/wav) and returns {"text": "..."}."""
    data = await request.body()
    if len(data) > _MAX_VOICE_BYTES:
        raise HTTPException(413, "Audio exceeds 2 MB limit.")
    if len(data) < 44:
        raise HTTPException(400, "Body too small to be a valid WAV.")

    model = _get_whisper_model()

    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        tmp.write(data)
        tmp_path = tmp.name

    try:
        segments, _ = model.transcribe(tmp_path, language="en", beam_size=1)
        text = " ".join(seg.text.strip() for seg in segments).strip()
    except Exception as e:
        logging.error(f"[Voice] Transcription failed: {e}")
        raise HTTPException(500, f"Transcription error: {e}")
    finally:
        Path(tmp_path).unlink(missing_ok=True)

    logging.info(f'[Voice] Transcribed: "{text}"')
    return {"text": text}
