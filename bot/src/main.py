#!/usr/bin/env python

import os
from telegram import Update
from telegram.ext import ApplicationBuilder, MessageHandler, filters, ContextTypes
from dotenv import load_dotenv
from datetime import datetime, timezone

from fastapi import FastAPI, UploadFile, File
import shutil

load_dotenv()
BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")

SAVE_PATH = "./received_photos"
if not os.path.exists(SAVE_PATH):
    os.makedirs(SAVE_PATH)

def get_save_dir():
    now = datetime.now(timezone.utc)
    y = now.strftime("%Y")
    m = now.strftime("%m")
    d = now.strftime("%d")
    ymd = now.strftime("%Y%m%d")
    h = now.strftime("%H")
    # Build path like "2025/11/30/20251130/15"
    dir_path = os.path.join("received_photos", y, m, d, ymd, h)
    # Create if not exist
    os.makedirs(dir_path, exist_ok=True)
    return dir_path

app = FastAPI()

@app.post("/sendphoto")
async def send_photo_endpoint(chat_id: int, ambient_light: str, file: UploadFile = File(...)):
    save_dir = get_save_dir()
    filename = file.filename
    filepath = os.path.join(save_dir, filename)
    with open(filepath, "wb") as buffer:
        shutil.copyfileobj(file.file, buffer)

    # Save ambient light info in .txt
    txtpath = filepath.rsplit('.', 1)[0] + ".txt"
    with open(txtpath, "w") as f:
        f.write(f"Ambient light: {ambient_light}\n")

    await send_photo_update(context=your_application_context, chat_id=chat_id, photo_path=filepath, caption=ambient_light)
    return {"status": "photo sent"}

async def photo_handler(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not update.message.photo:
        return

    photo = update.message.photo[-1]
    file = await context.bot.get_file(photo.file_id)

    save_dir = get_save_dir()

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    base_filename = f"{timestamp}-{photo.file_id}"

    photo_path = os.path.join(save_dir, base_filename + ".jpg")
    text_path = os.path.join(save_dir, base_filename + ".txt")

    await file.download_to_drive(photo_path)

    ambient_light = update.message.caption if update.message.caption else "No ambient light data"
    with open(text_path, "w") as f:
        f.write(f"{datetime.now(timezone.utc).isoformat()}\n")
        f.write(f"Ambient light: {ambient_light}\n")
    await update.message.reply_text(f"Photo and data saved as {base_filename} in {save_dir}")

async def main():
    application = ApplicationBuilder().token(BOT_TOKEN).build()
    application.add_handler(MessageHandler(filters.PHOTO, photo_handler))
    print("Bot started. Listening for photos...")
    await application.run_polling()

if __name__ == "__main__":
    import asyncio
    import sys

    application = ApplicationBuilder().token(BOT_TOKEN).build()
    application.add_handler(MessageHandler(filters.PHOTO, photo_handler))
    print("Bot started. Listening for photos...")

    try:
        application.run_polling()
    except (KeyboardInterrupt, SystemExit):
        print("Bot stopped")
        sys.exit()
