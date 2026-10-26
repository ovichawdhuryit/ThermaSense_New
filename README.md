# ThermaSense 🍲🤖

ThermaSense is an AI-powered food recognition and cooking-assistance system. It uses an ESP32-CAM to capture food images and sends them to a Flask backend hosted on Render. The backend uses Groq's vision AI model to identify the food and estimate cooking temperature and time.

## 🚀 Project Overview

ThermaSense combines AI, IoT, and temperature monitoring to create a smart cooking-assistance system.

### Main Features

- 📷 Food image capture using ESP32-CAM.
- 🧠 AI-based food recognition using Groq.
- 🌐 Flask backend hosted on Render.
- 🌡️ Temperature monitoring using an ESP8266 and DS18B20 sensor.
- 📟 LCD display for showing food and temperature information.
- 🔔 Buzzer and LEDs for status notifications.
- 🔌 Relay-based hardware control.
- 🔄 REST API communication between the hardware and backend.

## 🏗️ System Architecture

1. The ESP32-CAM captures a food image.
2. The image is uploaded to the Flask backend.
3. The backend sends the image to the Groq vision model.
4. The AI identifies the food and estimates a temperature and cooking time.
5. The backend applies configured temperature and time rules.
6. The prediction is returned as JSON.
7. The ESP8266 can retrieve the latest prediction through the `/state` endpoint.
8. The hardware displays information and monitors temperature.

## 🛠️ Technologies Used

- **ESP32-CAM** — Image capture
- **ESP8266** — IoT control
- **Python** — Backend programming
- **Flask** — REST API
- **Groq API** — Vision AI
- **Render** — Backend hosting
- **DS18B20** — Temperature sensing
- **LCD** — Information display
- **Relay, LEDs, and buzzer** — Hardware control

## 🌐 Deployed Backend

Base URL:

https://thermasense-l2ce.onrender.com

### API Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Opens the web dashboard |
| `/health` | GET | Checks whether the backend is responding |
| `/predict` | POST | Uploads an image and requests an AI prediction |
| `/state` | GET | Retrieves the latest prediction |
| `/last` | GET | Retrieves the latest uploaded image |

## 📤 Upload an Image Using Terminal

You can test the prediction API by uploading a local JPEG image directly from your Mac.

### Command

```bash
curl -i -X POST \
  -H "Content-Type: image/jpeg" \
  --data-binary "@/Users/anikchawdhury/Documents/Defence_Project/images/vaat.jpg" \
  "https://thermasense-l2ce.onrender.com/predict"
