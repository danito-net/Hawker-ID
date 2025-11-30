# Hawker-ID

This repository contains code for the Hawker-ID project, an intelligent ESP32-S3 AI camera system integrated with a Telegram bot for remote notifications and control. Telegram bot will deploy on Arduino UNO Q.

---

## Repository Structure

- **HawkerID/**  
  The ESP32-S3 firmware PlatformIO project.  
  This includes:  
  - Microphone loudness detection and clap-triggered photo capture.  
  - Camera initialization and photo saving to SD card.  
  - Ambient light sensor integration and IR LED control.  
  - Telegram bot integration for sending photos.

- **bot/**  
  Telegram bot code for the project (separate runtime environment).  
  Typically implemented with Python or Node.js.  
  Contains:  
  - Bot source code (`bot/src/`).  
  - Environment variable templates for bot token and configuration (`bot/.env.example`).  
  - Dependencies and instructions for running the bot.

---

## Firmware (HawkerID/)

### Building and Uploading

1. Open with PlatformIO in Visual Studio Code.  
2. Configure your WiFi credentials in `include/secrets.h` (not checked into Git, you can use `include/secrets.h.sample` by renaming it to `secrets.h`).  
3. Connect your ESP32-S3 AI CAM device.  
4. Build and upload firmware.  
5. Open Serial Monitor at 115200 baud to observe logs and debug output.

### Features

- Clap detection triggers camera captures saved locally to SD card.  
- Captured photos are also sent to a Telegram chat via the bot.  
- Ambient light sensor controls IR LED for low-light surveillance.  
- NTP time synchronization for timestamping images and logs.

---

## Telegram Bot (bot/)

### Setup

1. Copy `.env.example` to `.env` and set your Telegram bot token and chat ID.  
2. Copy `include/secrets.h.sample` to `include/secrets.h` and set your WiFi credentials.
3. Copy `include/telegram_secrets.h.sample` to `include/telegram_secrets.h` and set your Telegram bot token and chat ID. 
4. Install dependencies:

    cd bot
    pip install -r requirements.txt

5. Run the bot:

    python src/main.py


The bot receives photos from the device and sends notifications to Telegram users.

---

## Notes

- Keep all sensitive credentials out of source control using `.gitignore`.  
- Follow incremental commits with descriptive messages for feature stages.  
- For issues, consult the Wiki and official DFRobot ESP32-S3 AI CAM references:  
https://wiki.dfrobot.com/SKU_DFR1154_ESP32_S3_AI_CAM

---

## License (please see LICENSE file)

BSD 3-Clause License