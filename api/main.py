#!/usr/bin/env python3

import os
import shutil
import uvicorn

from datetime import datetime, timezone
from fastapi import FastAPI, UploadFile, File, Form
from fastapi.responses import JSONResponse

# Base directory where photos will be stored
SAVE_BASE = "./data"
os.makedirs(SAVE_BASE, exist_ok=True)

app = FastAPI(title="Hawker-ID Photo Receiver")

def get_save_dir() -> str:

    """Build a directory path by current WIB date and hour, for chronogically directories"""

    # now = datetime.now(timezone.utc)
    now = datetime.now()

    y = now.strftime("%Y")
    m = now.strftime("%m")
    d = now.strftime("%d")
    ymd = now.strftime("%Y%m%d")
    h = now.strftime("%H")

    dirpath = os.path.join(SAVE_BASE, y, m, d, ymd, h)
    os.makedirs(dirpath, exist_ok=True)
    return dirpath

@app.post("/api/sendphoto")

async def receive_photo(

    # These names MUST match the 'name' fields in the ESP32 C++ code
    chat_id: str = Form(...),
    ambient_light: str = Form(...),
    file: UploadFile = File(...)

):

    # Print to console so you know a request hit the server
    print(f"\n[HIT] Incoming photo from Chat ID: {chat_id}")

    try:

        savedir = get_save_dir()

        # now = datetime.now(timezone.utc)
        now = datetime.now()

        timestamp_str = now.strftime("%Y%m%d-%H%M%S")

        # Determine file extension
        original_name = file.filename or "photo.jpg"
        _, ext = os.path.splitext(original_name)

        if not ext:
            ext = ".jpg"

        base_name = f"{timestamp_str}"
        image_filename = base_name + ext
        image_path = os.path.join(savedir, image_filename)

        # 1. Save the Image
        with open(image_path, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)

        # 2. Save the Metadata Text File
        txt_filename = base_name + ".txt"
        txt_path = os.path.join(savedir, txt_filename)

        with open(txt_path, "w", encoding="utf-8") as f:
            f.write(f"Timestamp (UTC): {now.isoformat()}\n")
            f.write(f"Chat ID: {chat_id}\n")
            f.write(f"Ambient light: {ambient_light}\n")
            f.write(f"Original Filename: {original_name}\n")

        # Success Log
        print(f"[SUCCESS] Saved to: {image_path}")

        return JSONResponse(
            status_code=200,
            content={
                "status": "ok",
                "message": "Photo and data saved",
                "paths": {"image": image_path, "txt": txt_path}
            },
        )

    except Exception as e:
        print(f"[ERROR] {str(e)}")
        return JSONResponse(
            status_code=500,
            content={"status": "error", "message": str(e)},
        )

if __name__ == "__main__":
    # This block allows running via 'python3 main.py' directly if needed
    uvicorn.run(app, host="0.0.0.0", port=8000)
