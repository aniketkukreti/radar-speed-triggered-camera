/*
 * File: Radar_Server.ino
 * Author: aniketkukreti
 * Description:
 *  - Interfaces with Doppler radar sensor via UART
 *  - Calculates target speed
 *  - Logs overspeed violations
 *  - Hosts a web dashboard using ESP8266WebServer
 *
 * Hardware:
 *  - ESP8266
 *  - Doppler Radar Module
 *
 * Notes:
 *  - ESP32-CAM trigger logic can be integrated in processRadarData()
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

// ================= USER CONFIGURATION =================

// 1. WiFi Credentials
#include "config.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// 2. Settings
const float OVERSPEED_LIMIT = 30.00; // Limit in km/h
const char* locationName = "Main Gate"; // Location name to display on webpage

// 3. Pin Definitions
#define RADAR_RX_PIN 3
#define RADAR_TX_PIN 2

SoftwareSerial radarSerial(RADAR_RX_PIN, RADAR_TX_PIN);
ESP8266WebServer server(80);

// Radar Config Commands
byte cmdSpeedAngleSens[] = {0x43, 0x46, 0x01, 0x03, 0x19, 0x0A, 0x0D, 0x0A};
byte cmdTargetOutput[] = {0x43, 0x46, 0x02, 0x00, 0x01, 0x00, 0x0D, 0x0A};

// Data Variables
String radarBuffer = "";
float currentSpeed = 0.0;

// History Structure
struct Violation {
String time;
float speed;
};
Violation history[5];
unsigned long lastViolationTime = 0;

// HTML Page (Stored in Flash Memory)

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Radar Speed Trap</title>
<style>
body { font-family: 'Verdana', sans-serif; text-align: center; background-color: #2c3e50;
color: white; margin: 0; padding: 20px; }
.card { background: #34495e; max-width: 400px; margin: auto; padding: 20px;
border-radius: 15px; box-shadow: 0 10px 20px rgba(0,0,0,0.3); }
h1 { color: #ecf0f1; margin-bottom: 5px; font-size: 1.5rem; }
.location-tag { background: #3498db; color: white; padding: 4px 12px; border-radius: 20px;
font-size: 0.9rem; display: inline-block; margin-bottom: 20px; }
.speed-box { font-size: 4rem; font-weight: bold; color: #2ecc71; margin: 10px 0;
text-shadow: 2px 2px 4px #000; }
.danger { color: #e74c3c !important; animation: blink 0.5s infinite alternate; }
table { width: 100%; margin-top: 20px; border-collapse: collapse; background: #2c3e50;
border-radius: 8px; overflow: hidden; }
th, td { padding: 12px; border-bottom: 1px solid #465c71; text-align: center; font-size: 1rem;
}
th { background-color: #1abc9c; color: white; }
tr:last-child td { border-bottom: none; }
@keyframes blink { from { opacity: 1; } to { opacity: 0.3; } }
</style>
</head>
<body>
<div class="card">
<h1>SPEED MONITOR</h1>
<div id="locDisplay" class="location-tag">Loading...</div>
<div id="speedDisplay" class="speed-box">0.0</div>
<div style="color: #7f8c8d; font-size: 0.9rem;">KM/H</div>
<h3 style="margin-top: 30px; border-bottom: 1px solid #7f8c8d; padding-bottom:
10px;">VIOLATION LOG (>30)</h3>
<table id="logTable">
<tr><th>Log #</th><th>Recorded Speed</th></tr>
</table>
</div>
<script>
setInterval(function() {
fetch("/data").then(response => response.json()).then(json => {
const speedDiv = document.getElementById("speedDisplay");
speedDiv.innerHTML = json.speed.toFixed(1);
document.getElementById("locDisplay").innerHTML = "&#128205; " + json.loc;
if(json.speed > 30.0) {
speedDiv.classList.add("danger");
} else {
speedDiv.classList.remove("danger");
}
const table = document.getElementById("logTable");
while(table.rows.length > 1) { table.deleteRow(1); }
json.history.forEach((item, index) => {
let row = table.insertRow();
row.insertCell(0).innerHTML = item.id;
row.insertCell(1).innerHTML = "<span style='color:#e74c3c; font-weight:bold'>" + item.spd
+ " km/h</span>";
});
}).catch(e => console.log(e));
}, 500);
</script>
</body>
</html>
)rawliteral";
void setup() {
Serial.begin(115200);
radarSerial.begin(9600);
Serial.println("\n\n=== Initializing Speed Trap ===");


// Radar Initialization

sendCommand(cmdSpeedAngleSens, sizeof(cmdSpeedAngleSens), "Set Sensitivity/Angle");
sendCommand(cmdTargetOutput, sizeof(cmdTargetOutput), "Set Output Mode");

// WiFi Setup

Serial.print("Connecting to WiFi: ");
Serial.println(ssid);
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}
Serial.println("\nWiFi connected!");
Serial.print("Dashboard available at: http://");
Serial.println(WiFi.localIP());
server.on("/", handleRoot);
server.on("/data", handleData);
server.begin();
}
void loop() {
server.handleClient();
readRadar();
}
void handleRoot() {
server.send_P(200, "text/html", index_html);
}
void handleData() {
String json = "{";
json += "\"speed\":" + String(currentSpeed, 1) + ",";
json += "\"loc\":\"" + String(locationName) + "\",";
json += "\"history\":[";
for(int i = 0; i < 5; i++) {
if(history[i].speed > 0) {
json += "{\"id\":" + String(i+1) + ",";
json += "\"spd\":" + String(history[i].speed, 1) + "}";
if(i < 4 && history[i+1].speed > 0) json += ",";
}
}
json += "]}";
server.send(200, "application/json", json);
}
void readRadar() {
while (radarSerial.available()) {
char c = radarSerial.read();
if (c == '\n') {
radarBuffer.trim();
processRadarData(radarBuffer);
radarBuffer = "";
} else {
radarBuffer += c;
}
}
}
void processRadarData(String data) {
if (data.length() < 3 || data.charAt(0) != 'V') return;
float speed = data.substring(2).toFloat();
currentSpeed = speed;
Serial.print("Target Detected: ");
Serial.print(speed, 1);
Serial.println(" km/h");
// Violation Logic
if (speed > OVERSPEED_LIMIT) {
if (millis() - lastViolationTime > 2000) {
logViolation(speed);
lastViolationTime = millis();
Serial.println("!!! VIOLATION LOGGED !!!");
// TODO: Trigger ESP32-CAM capture on overspeed event}
}
}
void logViolation(float spd) {
for (int i = 4; i > 0; i--) {
history[i] = history[i-1];
}
history[0].speed = spd;
}
void sendCommand(byte *cmd, size_t len, const char *desc) {
for (size_t i = 0; i < len; i++) radarSerial.write(cmd[i]);
delay(100);
}
