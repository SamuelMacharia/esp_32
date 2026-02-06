| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

# ESP32 Camera Streaming and Face Recognition Using OV3660

This project turns an esp32-cam to a real-time streaming server with the groundwork for face recognition. It uses:
    FreeRTOS tasks for concurrency
    ESP-IDF for full control of peripherals
    Queues and Event Groups for safe inter-task communication

The system captures camera frames, streams them to clients over HTTP, and is designed to scale to multiple clients without crashing or blocking.

---

## Features

- Real-time video streaming from ESP32-CAM  
- WiFi connection with retry logic and event handling  
- Task-based architecture with FreeRTOS  
- Queue-based frame buffer management to prevent memory issues  
- Event groups flags for synchronizing WiFi, streaming, and capture  
- Modular design for future extensions (MQTT, Face Recognition, Power Management)  

---

## System Architecture

[Camera] --> [Capture Task] --> [Frame Queue] --> [Stream Task] --> [HTTP Client]
[WiFi] --> [Event Group] --> Signals readiness


- Event Groups are used to:  
  - Signal when WiFi is connected (`WIFI_CONNECTED_BIT`) or failed (`WIFI_FAILED_BIT`)  
  - Signal when a stream client is ready (`STREAM_READY_BIT`)  
- Queues store camera frames between capture task and streaming task to avoid race conditions and lost frames  
- FreeRTOS tasks run concurrently  

---

## Hardware

- ESP32-CAM (AI-Thinker)  
- PSRAM for higher resolution frames  
- Pin configuration defined in `pins.h`  

---

## Software Setup

1. Install **ESP-IDF** and set up your environment  
2. Clone the repository:  
   ```bash
   git clone https://github.com/SamuelMacharia/esp_32.git
   cd esp_32

3. Configure WiFi in menuconfig (CONFIG_WIFI_SSID & CONFIG_WIFI_PASSWORD)

4. Build and flash:

  ```bash
   $HOME/esp/esp-idf/export.sh
   idf.py build
   idf.py flash
   idf.py monitor
   

5. Open a browser and go to:  
  ```bash
  http://<Your_IP>/stream


