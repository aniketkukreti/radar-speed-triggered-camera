/*
 * File: esp32_cam_client.ino
 * Author: aniketkukreti
 * Description:
 *  - Captures images using ESP32-CAM (AI Thinker)
 *  - Sends JPEG frames to a Flask backend via HTTP POST
 *  - Receives ANPR result (license plate number)
 *
 * Hardware:
 *  - ESP32-CAM (AI Thinker)
 *
 * Backend:
 *  - Flask server with YOLO + Tesseract OCR
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// Configuration
#include "src/config.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* serverUrl = SERVER_URL;

// Pin Definitions for AI Thinker ESP32-CAM

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
void setup() {
Serial.begin(115200);
Serial.println("\nESP32-CAM License Plate Detector");
camera_config_t config;
config.ledc_channel = LEDC_CHANNEL_0;
config.ledc_timer = LEDC_TIMER_0;
config.pin_d0 = Y2_GPIO_NUM;
config.pin_d1 = Y3_GPIO_NUM;
config.pin_d2 = Y4_GPIO_NUM;
config.pin_d3 = Y5_GPIO_NUM;
config.pin_d4 = Y6_GPIO_NUM;
config.pin_d5 = Y7_GPIO_NUM;
config.pin_d6 = Y8_GPIO_NUM;
config.pin_d7 = Y9_GPIO_NUM;
config.pin_xclk = XCLK_GPIO_NUM;
config.pin_pclk = PCLK_GPIO_NUM;
config.pin_vsync = VSYNC_GPIO_NUM;
config.pin_href = HREF_GPIO_NUM;
config.pin_sscb_sda = SIOD_GPIO_NUM;
config.pin_sscb_scl = SIOC_GPIO_NUM;
config.pin_pwdn = PWDN_GPIO_NUM;
config.pin_reset = RESET_GPIO_NUM;
config.xclk_freq_hz = 20000000;
config.pixel_format = PIXFORMAT_JPEG;
if(psramFound()){
config.frame_size = FRAMESIZE_UXGA; // 1600x1200
config.jpeg_quality = 10;
config.fb_count = 2;
} else {
config.frame_size = FRAMESIZE_SVGA; // 800x600
config.jpeg_quality = 12;
config.fb_count = 1;
}
esp_err_t err = esp_camera_init(&config);
if (err != ESP_OK) {
Serial.printf("Camera init failed with error 0x%x", err);
return;
}
Serial.println("Camera initialized!");
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}
Serial.println("\nWiFi connected!");
}
void loop() {

// Check WiFi connection
if(WiFi.status() != WL_CONNECTED) {
WiFi.begin(ssid, password);
delay(5000);
return;
}
Serial.println("\n--- Capturing image ---");
camera_fb_t * fb = esp_camera_fb_get();
if(!fb) {
Serial.println("Camera capture failed");
delay(1000);
return;
}
Serial.printf("Image captured! Size: %d bytes\n", fb->len);

// POST Image to Server
HTTPClient http;
http.begin(serverUrl);
http.addHeader("Content-Type", "image/jpeg");
http.setTimeout(15000);
Serial.println("Sending image to server...");
int httpResponseCode = http.POST(fb->buf, fb->len);
if(httpResponseCode > 0) {
String response = http.getString();
Serial.printf("Response code: %d\n", httpResponseCode);
Serial.println("Server response: " + response);
// Parse JSON Response for 'plate'
int plateStart = response.indexOf("\"plate\":\"") + 9;
if(plateStart > 8) {
int plateEnd = response.indexOf("\"", plateStart);
String plateNumber = response.substring(plateStart, plateEnd);
Serial.println(">>> DETECTED PLATE: " + plateNumber + " <<<");
}
} else {
Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
}
http.end();
esp_camera_fb_return(fb);
Serial.println("Waiting 10 seconds before next capture...");
delay(10000);
}