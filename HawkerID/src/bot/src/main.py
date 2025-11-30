#!/usr/bin/env python

import os
from telegram import Update
from telegram.ext import ApplicationBuilder, MessageHandler, filters, ContextTypes
from dotenv import load_dotenv
from datetime import datetime

load_dotenv()
BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")

SAVE_PATH = "./received_photos"
if not os.path.exists(SAVE_PATH):
    os.makedirs(SAVE_PATH)

async def photo_handler(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not update.message.photo:
        return
    
    # Get highest resolution photo
    photo = update.message.photo[-1]
    file = await context.bot.get_file(photo.file_id)
    
    # Build filename base using current UTC time and photo file_id
    timestamp = datetime.utcnow().strftime("%Y%m%d-%H%M%S")
    base_filename = f"{timestamp}-{photo.file_id}"
    
    photo_path = os.path.join(SAVE_PATH, base_filename + ".jpg")
    text_path = os.path.join(SAVE_PATH, base_filename + ".txt")
    
    # Download photo (binary)
    await file.download_to_drive(photo_path)
    
    # For demo, get caption text or default message for .txt file
    ambient_light = update.message.caption if update.message.caption else "No ambient light data"
    with open(text_path, "w") as f:
        # Save date-time stamp + ambient light (caption) here
        f.write(f"{datetime.utcnow().isoformat()} UTC\n")
        f.write(f"Ambient light: {ambient_light}\n")
    
    await update.message.reply_text(f"Photo and data saved as {base_filename}")

async def main():
    application = ApplicationBuilder().token(BOT_TOKEN).build()
    application.add_handler(MessageHandler(filters.PHOTO, photo_handler))
    print("Bot started. Listening for photos...")
    await application.run_polling()

if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
