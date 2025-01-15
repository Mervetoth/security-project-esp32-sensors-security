#include <WiFi.h>
#include <EEPROM.h>
#include <WebServer.h>
#include "DHT.h"
#include <Adafruit_Sensor.h>

// Wi-Fi credentials
const char* ssid = "HUAWEI-2.4G-pK26";
const char* password = "mTFhVa3j";

// Pin definitions
#define RED_PIN 14
#define GREEN_PIN 12
#define BLUE_PIN 13
#define BUTTON_PIN 2
#define TRIG_PIN 5
#define ECHO_PIN 18
#define DHT_PIN 27
#define LED_PIN 2

// Create a web server on port 80
WebServer server(80);

// Variables
bool ledState = false;
bool manualMode = false;
long duration;
int distance;
float humidity = 0.0, temperature = 0.0;

// DHT sensor setup
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// HTML for interface
String getHTML() {
  // Your existing HTML code here (no changes)
  String html = "<!DOCTYPE html><html><head><title>ESP32 Control</title><style>";
  html += "body { font-family: Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f8ff; margin: 0; }";
  html += ".container { background-color: #DAEAF6; padding: 20px; border: 1px solid gray; border-radius: 10px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); width: 300px; text-align: center; display: flex; flex-direction: column; align-items: center; justify-content: center; }";
  html += "h1 { font-size: 24px; margin-bottom: 20px; }";
  html += "label, select { font-size: 16px; margin-bottom: 15px; display: block; text-align: center; }";
  html += "button { padding: 10px 20px; font-size: 16px; margin: 10px 0; cursor: pointer; border-radius: 5px; border: 1px solid gray; background-color: #fff; }";
  html += ".toggle { position: relative; width: 50px; height: 25px; margin: 10px 0; }";
  html += ".toggle input { opacity: 0; width: 0; height: 0; }";
  html += ".slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: gray; transition: 0.4s; border-radius: 25px; }";
  html += ".slider:before { position: absolute; content: ''; height: 21px; width: 21px; left: 2px; bottom: 2px; background-color: white; transition: 0.4s; border-radius: 50%; }";
  html += "input:checked + .slider { background-color: green; }";
  html += "input:checked + .slider:before { transform: translateX(25px); }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP32 Multi-sensor Control</h1>";
  html += "<label for='mode'>Select Mode:</label>";
  html += "<select name='mode' id='mode' onchange='setMode(this.value)'>";
  html += "<option value='manual' " + String(manualMode ? "selected" : "") + ">Manual Mode</option>";
  html += "<option value='obstacle' " + String(!manualMode && !isnan(distance) ? "selected" : "") + ">Obstacle Detection</option>";
  html += "<option value='humidity' " + String(!manualMode && isnan(distance) ? "selected" : "") + ">Humidity/Temperature</option>";
  html += "</select>";
  html += "<p id='ledStatus' style='font-size:18px;'>LED is currently " + String(ledState ? "ON" : "OFF") + "</p>";
  
  if (manualMode) {
    html += "<label for='ledToggle'>LED Control:</label>";
    html += "<label class='toggle'>";
    html += "<input type='checkbox' id='ledToggle' onclick='toggleLED()' " + String(ledState ? "checked" : "") + ">";
    html += "<span class='slider'></span>";
    html += "</label>";
  }

  html += "<button onclick='toggleSensorValues()'>Show/Hide Sensor Values</button>";
  html += "<div id='sensorValues' style='margin-top: 20px; display: none;'>";
  html += "<p style='font-size: 16px;'>Humidity: " + (isnan(humidity) ? "Error" : String(humidity)) + "%</p>";
  html += "<p style='font-size: 16px;'>Temperature: " + (isnan(temperature) ? "Error" : String(temperature)) + " &#8451;</p>";
  html += "</div>";
  
  html += "<script>";
  html += "function setMode(mode) { fetch('/setMode?mode=' + mode).then(() => location.reload()); }";
  html += "function toggleLED() {";
  html += "  fetch('/toggle').then(() => {";
  html += "    fetch('/getLEDState').then(response => response.text()).then(text => {";
  html += "      document.getElementById('ledStatus').innerHTML = 'LED is currently ' + text;";
  html += "    });";
  html += "  });";
  html += "}";
  html += "function toggleSensorValues() {";
  html += "  var sensorValues = document.getElementById('sensorValues');";
  html += "  if (sensorValues.style.display === 'none') {";
  html += "    sensorValues.style.display = 'block';";
  html += "  } else {";
  html += "    sensorValues.style.display = 'none';";
  html += "  }";
  html += "}";
  html += "</script>";
  
  html += "</div>";
  html += "</body></html>";
  return html;
}

// Handle root
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// Handle LED toggle
void handleToggle() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  EEPROM.write(0, ledState);  // Save LED state to EEPROM (address 0)
  EEPROM.commit();  // Commit changes to EEPROM
  server.send(200, "text/plain", "OK");
}

// Handle mode changes based on dropdown selection
void handleSetMode() {
  String mode = server.hasArg("mode") ? server.arg("mode") : "manual";
  if (mode == "manual") {
    manualMode = true;
    setRGB(255, 0, 0);  // Red for manual mode
  } else if (mode == "obstacle") {
    manualMode = false;
    setRGB(0, 255, 0);  // Green for obstacle detection
  } else if (mode == "humidity") {
    manualMode = false;
    setRGB(255, 0, 255);  // Red+Blue for humidity/temp
  }
  EEPROM.write(1, manualMode);  // Save manual mode state to EEPROM (address 1)
  EEPROM.commit();  // Commit changes to EEPROM
  server.send(200, "text/html", getHTML());
}

// Set RGB LED color
void setRGB(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

// Obstacle detection
void obstacleDetection() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;
  if (distance < 15) {
    digitalWrite(LED_PIN, HIGH);  // Turn on LED if obstacle is close
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

// Get DHT11 data with error handling
void getDHTData() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor! Retrying...");
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
  }
}

// Setup
void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();
  Serial.begin(115200);

  EEPROM.begin(512);  // Initialize EEPROM with 512 bytes of space

  // Read stored values from EEPROM
  ledState = EEPROM.read(0);  // Read LED state from EEPROM (address 0)
  manualMode = EEPROM.read(1);  // Read manual mode state from EEPROM (address 1)
  
  // Set initial LED state based on EEPROM value
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/setMode", handleSetMode);

  server.on("/getLEDState", []() {
    server.send(200, "text/plain", ledState ? "ON" : "OFF");
  });

  server.begin();
  Serial.println("HTTP server started");
}

// Loop
void loop() {
  server.handleClient();

  if (!manualMode) {
    obstacleDetection();
    getDHTData();
  }
}
