#!/usr/bin/env python3

import os
import sys
import logging
from datetime import datetime

from telegram import Update
from telegram.ext import ApplicationBuilder, MessageHandler, filters, ContextTypes
from telegram.request import HTTPXRequest
from dotenv import load_dotenv


logging.basicConfig(
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    level=logging.INFO
)

logger = logging.getLogger(__name__)

load_dotenv()
BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")

# Base directory
SAVE_PATH = "./data"
if not os.path.exists(SAVE_PATH):
    os.makedirs(SAVE_PATH)

def get_save_dir():

    """Returns directory path based on LOCAL System Time (WIB)."""

    now = datetime.now()

    y = now.strftime("%Y")
    m = now.strftime("%m")
    d = now.strftime("%d")
    ymd = now.strftime("%Y%m%d")
    h = now.strftime("%H")

    dir_path = os.path.join(SAVE_PATH, y, m, d, ymd, h)
    os.makedirs(dir_path, exist_ok=True)

    return dir_path

async def photo_handler(update: Update, context: ContextTypes.DEFAULT_TYPE):

    """Handles both compressed Photos and uncompressed Document images."""

    logger.info("Received a message with photo/document.")

    # Determine if it's a Photo (compressed) or Document (file)
    if update.message.photo:

        # Take the last (highest resolution) photo
        photo_obj = update.message.photo[-1]
        file_id = photo_obj.file_id
        logger.info(f"Processing Compressed Photo. ID: {file_id}")

    elif update.message.document:
        photo_obj = update.message.document
        file_id = photo_obj.file_id
        logger.info(f"Processing Document Image. ID: {file_id}")

    else:
        return

    try:

        # Get the file object from Telegram
        file = await context.bot.get_file(file_id)

        # Prepare paths
        save_dir = get_save_dir()
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        base_filename = f"{timestamp}-{file_id[:10]}"

        photo_path = os.path.join(save_dir, base_filename + ".jpg")
        text_path = os.path.join(save_dir, base_filename + ".txt")

        # Download
        logger.info(f"Downloading to: {photo_path}")
        await file.download_to_drive(photo_path)
        logger.info("Download successful.")

        # Handle caption
        ambient_light = update.message.caption if update.message.caption else "No ambient light data (User Upload)"

        # Save metadata
        with open(text_path, "w") as f:
            f.write(f"{datetime.now().isoformat()}\n")
            f.write(f"Ambient light: {ambient_light}\n")

        # Reply to user
        await update.message.reply_text(f"Saved: {base_filename}.jpg")
        logger.info("Reply sent to user.")

    except Exception as e:

        logger.error(f"Failed to save photo: {e}", exc_info=True)
        await update.message.reply_text("Error: Could not save photo. Check server logs.")

async def error_handler(update: object, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Log the error and send a telegram message to notify the developer."""
    logger.error("Exception while handling an update:", exc_info=context.error)

if __name__ == "__main__":

    if not BOT_TOKEN:
        logger.critical("TELEGRAM_BOT_TOKEN is missing!")
        sys.exit(1)

    # Increase timeouts to 60s to prevent 'TimedOut' errors
    req = HTTPXRequest(connect_timeout=60, read_timeout=60)

    application = ApplicationBuilder().token(BOT_TOKEN).request(req).build()

    # Add handlers
    # filters.PHOTO catches compressed images
    # filters.Document.IMAGE catches uncompressed images sent as files
    application.add_handler(MessageHandler(filters.PHOTO | filters.Document.IMAGE, photo_handler))

    # Add error handler
    application.add_error_handler(error_handler)

    logger.info("Bot started. Polling...")

    try:
        application.run_polling()
    except Exception as e:
        logger.critical(f"Bot crashed: {e}")
