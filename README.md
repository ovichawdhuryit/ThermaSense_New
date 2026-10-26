# ThermaSense — AI-Powered Food Recognition and Cooking Assistant

ThermaSense is an IoT-based food recognition and cooking assistance system. It uses an ESP32-CAM to capture food images and send them to a Flask backend hosted on Render. The backend uses a Groq-hosted vision model to identify the food and estimate a suitable cooking temperature and cooking time. The latest prediction can then be retrieved by an ESP8266 or other connected device.

## 1. Project Overview

ThermaSense consists of the following components:

- **ESP32-CAM:** Captures food images and sends JPEG data to the backend.
- **Flask Backend:** Receives images, calls the Groq API, processes the response, and stores the latest prediction.
- **Groq Vision Model:** Analyzes the image and predicts the food name, temperature, and cooking time.
- **Render:** Hosts the Flask backend online.
- **ESP8266:** Retrieves the latest prediction from the backend.
- **Dashboard:** Displays the latest prediction and provides access to the most recently uploaded image.

### System Workflow

1. The ESP32-CAM captures an image of food.
2. The image is uploaded to the Flask backend.
3. The backend sends the image to the Groq vision model.
4. The model returns the predicted food name, temperature, and cooking time.
5. The backend processes and stores the prediction.
6. The ESP8266 retrieves the latest prediction from the `/state` endpoint.
7. The dashboard displays the latest result.

## 2. Technologies Used

- ESP32-CAM
- ESP8266
- Python
- Flask
- Groq API
- Vision-language model
- Gunicorn
- Render
- HTTP/REST API
- JSON
- JPEG image processing

## 3. Backend Deployment

The Flask backend is deployed on Render.

**Backend URL:**

https://thermasense-l2ce.onrender.com

### Available Endpoints

| Method | Endpoint | Description |
|---|---|---|
| GET | `/` | Opens the dashboard |
| GET | `/health` | Checks backend availability |
| POST | `/predict` | Uploads a JPEG image for prediction |
| GET | `/state` | Returns the latest prediction as JSON |
| GET | `/last` | Returns the most recently uploaded image |

### Check Backend Health

Open the following URL in a browser:

https://thermasense-l2ce.onrender.com/health

The endpoint should return a response indicating whether the backend is running.

### Open the Dashboard

https://thermasense-l2ce.onrender.com/

## 4. Upload a Food Image Using cURL

The backend accepts raw JPEG image data through the `/predict` endpoint.

### Local Image Path

The test image is located at:

`pathname`

### Upload Command

Run the following command in the terminal:

```bash
curl -i -X POST \
  -H "Content-Type: image/jpeg" \
  --data-binary "@pathname" \
  "https://thermasense-l2ce.onrender.com/predict"
```

### Explanation

- `-i` includes the HTTP response headers.
- `-X POST` sends a POST request.
- `Content-Type: image/jpeg` identifies the uploaded image format.
- `--data-binary` sends the image bytes without converting them into text.
- `/predict` is the backend endpoint responsible for processing the image.

**Important:** Replace the local image path if your test image is stored somewhere else.

## 5. Retrieve the Latest Prediction

After uploading an image, use the following command to retrieve the latest prediction:

```bash
curl -i "https://thermasense-l2ce.onrender.com/state"
```

The response should contain the latest prediction in JSON format.

The exact JSON fields depend on the structure implemented in `app.py`.

### Example Prediction Format

```json
{
  "food": "Rice",
  "temperature": 75,
  "time": 10
}
```

This JSON is an illustrative example. The actual response depends on the backend implementation and model prediction.

## 6. View the Latest Uploaded Image

To retrieve the most recently uploaded image, open:

https://thermasense-l2ce.onrender.com/last

This endpoint returns the latest image if the backend has one available.

## 7. Groq API Configuration

The backend uses the Groq API to analyze uploaded images.

**API endpoint:**

`https://api.groq.com/openai/v1/chat/completions`

The API key should be stored as an environment variable rather than hardcoded into the source code.

### Required Environment Variables

```text
GROQ_API_KEY=your_groq_api_key
MODEL=your_supported_vision_model_id
```

Replace the example values with your actual Groq API key and the exact model ID available to your account.

### Important Model Configuration

The selected model must support image inputs and be available through the Groq API.

If the backend returns a `model_not_found` error, check:

1. The exact model ID.
2. Whether the model is currently available through Groq.
3. Whether the model supports vision/image inputs.
4. Whether your account has access to the model.
5. Whether the `MODEL` environment variable matches the model used by the backend.

Do not assume that a model is available just because a similarly named model appears in a model list.

## 8. Backend Response Format

The backend expects the vision model to return a prediction in this format:

```text
Food: <name> --- Temperature: <number>°C --- Time: <number> minutes
```

### Example

```text
Food: Rice --- Temperature: 75°C --- Time: 10 minutes
```

The backend parses the model response to extract the food name, temperature, and cooking time.

If parsing fails, the current fallback behavior uses:

- Food: Unknown
- Temperature: 75°C
- Time: 10 minutes

The backend also uses configured `SAFE_RANGE` rules to constrain the predicted temperature and cooking time.

**Safety note:** Model-generated temperatures and times are estimates, not verified food-safety instructions. Appropriate cooking conditions depend on the food, its thickness, preparation method, and other factors. Use a suitable food thermometer and reliable food-safety guidance where applicable.

## 9. Render Deployment Configuration

The backend is hosted on Render.

### Start Command

```bash
gunicorn app:app --workers 1
```

This command assumes that:

- The Python file is named `app.py`.
- The Flask application object is named `app`.
- Gunicorn is included in the project's dependencies.

### Typical Dependencies

Check that your project includes the packages it actually uses. A typical `requirements.txt` may include:

```text
Flask
gunicorn
requests
Pillow
```

Add any additional dependencies required by your implementation.

### Deployment Checklist

- [ ] The source code is available in the connected repository.
- [ ] The correct Python version is configured.
- [ ] Dependencies are listed in `requirements.txt`.
- [ ] `GROQ_API_KEY` is configured in Render environment variables.
- [ ] `MODEL` contains a valid, supported vision-model ID.
- [ ] The start command is configured correctly.
- [ ] The deployment completes successfully.
- [ ] `/health` responds successfully.
- [ ] `/predict` accepts JPEG images.
- [ ] `/state` returns the latest prediction.
- [ ] `/last` returns the latest image when available.

## 10. ESP32-CAM Integration

The ESP32-CAM captures an image and sends its JPEG bytes to:

`https://thermasense-l2ce.onrender.com/predict`

The request must use:

```text
Method: POST
Content-Type: image/jpeg
Body: Raw JPEG image bytes
```

The ESP32-CAM should verify the HTTP response and handle failed requests appropriately.

### Recommended Request Flow

1. Initialize the ESP32-CAM.
2. Capture a frame.
3. Verify that the captured frame is valid.
4. Connect to Wi-Fi.
5. Send the JPEG data to the backend.
6. Check the HTTP status code.
7. Read the response.
8. Release the captured frame buffer.
9. Retry later if the request fails.

The ESP32-CAM must send the image in the format expected by the backend. If the backend expects raw JPEG bytes, do not send a JSON object or multipart form unless the backend is modified to accept that format.

## 11. ESP8266 Integration

The ESP8266 can retrieve the latest prediction from:

`https://thermasense-l2ce.onrender.com/state`

### Recommended Request Flow

1. Connect the ESP8266 to Wi-Fi.
2. Send an HTTP GET request to `/state`.
3. Read the JSON response.
4. Parse the fields that exist in the actual response.
5. Use the prediction in the connected application or device.
6. Handle network errors and invalid responses.

The ESP8266 should not assume that every request will succeed. It should handle timeouts, HTTP errors, and missing JSON fields.

## 12. Testing the Complete System

### Test 1: Backend Availability

Open:

https://thermasense-l2ce.onrender.com/health

Verify that the backend responds.

### Test 2: Image Upload

Run the cURL command from Section 4.

Verify that the backend accepts the request and returns an appropriate response.

### Test 3: Prediction Retrieval

Run:

```bash
curl -i "https://thermasense-l2ce.onrender.com/state"
```

Verify that the latest prediction is returned.

### Test 4: Latest Image

Open:

https://thermasense-l2ce.onrender.com/last

Verify that the latest uploaded image is available.

### Test 5: Dashboard

Open:

https://thermasense-l2ce.onrender.com/

Verify that the dashboard loads and displays the available information.

### Test 6: Hardware Integration

Test the ESP32-CAM image upload and the ESP8266 prediction retrieval separately before testing the complete workflow.

## 13. Troubleshooting

### Problem: `model_not_found`

**Possible causes:**

- Incorrect model ID.
- Model unavailable through Groq.
- Model access restrictions.
- Unsupported model capability.

**Solution:** Verify the exact model ID and confirm that the selected model supports vision inputs.

### Problem: `401 Unauthorized`

**Possible causes:**

- Invalid API key.
- Missing `GROQ_API_KEY`.
- Incorrect environment-variable configuration.

**Solution:** Verify the API key in Render's environment settings. Never publish the actual API key in the repository or README.

### Problem: `429 Too Many Requests`

**Possible causes:**

- API rate limits.
- Usage limits.
- Too many requests in a short period.

**Solution:** Check the API response, review account limits, and implement appropriate retry delays.

### Problem: Image Upload Fails

**Possible causes:**

- Incorrect local file path.
- Invalid image file.
- Incorrect `Content-Type`.
- Network or server error.

**Solution:** Confirm that the image exists and is a valid JPEG, then retry the cURL command.

### Problem: `/state` Returns Unexpected Data

**Possible causes:**

- No prediction has been generated yet.
- The backend uses different JSON field names.
- Prediction parsing failed.
- The latest request failed.

**Solution:** Inspect the backend response and Render logs. Verify that the ESP8266 parses the actual JSON structure returned by `app.py`.

### Problem: Backend Is Slow or Unavailable

A hosted service may take time to respond, particularly after a period without requests.

**Solution:** Check the Render deployment logs and health endpoint. Add suitable client-side timeouts and error handling.

## 14. Security Considerations

- Store API keys in environment variables.
- Never commit secrets to a public repository.
- Validate uploaded image data.
- Handle malformed requests safely.
- Set appropriate request timeouts.
- Avoid logging sensitive credentials.
- Handle API failures and unexpected model responses.
- Validate temperature and cooking-time values before using them in hardware control.
- Do not rely on AI predictions alone for food-safety decisions.

## 15. Future Improvements

Potential improvements include:

- More reliable food classification.
- Better handling of ambiguous food images.
- Support for additional food categories.
- Improved prediction validation.
- ESP32-CAM upload retries.
- ESP8266 connection recovery.
- More informative dashboard displays.
- Prediction history.
- Logging and monitoring.
- Improved response-time management.
- Integration with suitable temperature sensors.
- Testing predictions against verified cooking procedures.

## 16. Project Summary

ThermaSense combines embedded hardware, computer vision, a cloud-hosted backend, and an API-based AI model to provide food-recognition and cooking assistance.

The ESP32-CAM captures food images, the Flask backend processes them through the Groq API, and the latest prediction is made available to the dashboard and ESP8266.

The project provides a foundation for further development of an IoT-based food-recognition and cooking-assistance system.
