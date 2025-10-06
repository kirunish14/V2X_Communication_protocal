#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// WiFi credentials
const char* ssid = "Jayy";
const char* password = "Jayaraj@231";

// MQTT broker
const char* mqtt_server = "broker.emqx.io";

// Twilio credentials (fill with your info)
const char* twilio_sid = "AC3d97b802699822348c5e0bacb3287db6";
const char* twilio_auth_token = "43a11f52ca9095a4362c3b5de304c353";
const char* twilio_phone = "+15413488576"; // Your Twilio phone number

// Alert target phone numbers
const char* alertNumbers[3] = {
  "+919626849097",
  "+916282556241",
  "+919176112006"
};

#define SDA_PIN 25
#define SCL_PIN 26

Adafruit_MPU6050 mpu;
WiFiClientSecure secureClient;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

// Motor pins
int enableRightMotor = 22;
int rightMotorPin1 = 16;
int rightMotorPin2 = 17;
int enableLeftMotor = 23;
int leftMotorPin1 = 18;
int leftMotorPin2 = 19;

// Ultrasonic pins
const int trigPin = 5;
const int echoPin = 4;
const long safeDistance = 15;
bool obstacleDetected = false;

// Road detection filter buffer
const int FILTER_SIZE = 10;
float zBuffer[FILTER_SIZE];
int zIndex = 0;
bool zBufferFilled = false;

// Variables to store subscribed data
String latestInterruptMessage = "No alerts yet.";
String trafficColor = "RED";
int trafficTime = 0;
String parkingSlots = "-- / --";
String sensorJson = "{}";
String speedBreakerMessage = "";

// Track last received traffic signal to detect disconnection
unsigned long lastTrafficSignalTime = 0;
const unsigned long TRAFFIC_TIMEOUT_MS = 5000;  // 5 seconds timeout

// Alert state
bool alertSent = false;

// --- Helper Functions ---
void stopCar() {
  digitalWrite(rightMotorPin1, LOW);
  digitalWrite(rightMotorPin2, LOW);
  digitalWrite(leftMotorPin1, LOW);
  digitalWrite(leftMotorPin2, LOW);
  ledcWrite(enableRightMotor, 0);
  ledcWrite(enableLeftMotor, 0);
}

long getDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  long distance = duration * 0.034 / 2;
  return distance;
}

void forward() {
  if (obstacleDetected) {
    stopCar();
    return;
  }
  digitalWrite(rightMotorPin1, HIGH);
  digitalWrite(rightMotorPin2, LOW);
  digitalWrite(leftMotorPin1, HIGH);
  digitalWrite(leftMotorPin2, LOW);
  ledcWrite(enableRightMotor, 120);
  ledcWrite(enableLeftMotor, 120);
}

void backward() {
  digitalWrite(rightMotorPin1, LOW);
  digitalWrite(rightMotorPin2, HIGH);
  digitalWrite(leftMotorPin1, LOW);
  digitalWrite(leftMotorPin2, HIGH);
  ledcWrite(enableRightMotor, 120);
  ledcWrite(enableLeftMotor, 120);
}

void moveLeft() {
  if (obstacleDetected) {
    stopCar();
    return;
  }
  digitalWrite(rightMotorPin1, HIGH);
  digitalWrite(rightMotorPin2, LOW);
  digitalWrite(leftMotorPin1, LOW);
  digitalWrite(leftMotorPin2, HIGH);
  ledcWrite(enableRightMotor, 90);
  ledcWrite(enableLeftMotor, 90);
}

void moveRight() {
  if (obstacleDetected) {
    stopCar();
    return;
  }
  digitalWrite(rightMotorPin1, LOW);
  digitalWrite(rightMotorPin2, HIGH);
  digitalWrite(leftMotorPin1, HIGH);
  digitalWrite(leftMotorPin2, LOW);
  ledcWrite(enableRightMotor, 90);
  ledcWrite(enableLeftMotor, 90);
}

float getFilteredZ(float rawZ) {
  zBuffer[zIndex++] = rawZ;
  if (zIndex >= FILTER_SIZE) {
    zIndex = 0;
    zBufferFilled = true;
  }
  int count = zBufferFilled ? FILTER_SIZE : zIndex;
  float sum = 0;
  for (int i = 0; i < count; i++) sum += zBuffer[i];
  return sum / count;
}

String detectRoadCondition(float filteredZ) {
  if (filteredZ < -9.9 || filteredZ > -9.4) {
    return "⚠️ Speed Breaker Ahead!";
  }
  return "";
}

// --- MQTT Functions ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  if (topicStr == "car1/control") {
    if (message == "forward") forward();
    else if (message == "backward") backward();
    else if (message == "left") moveLeft();
    else if (message == "right") moveRight();
    else if (message == "stop") stopCar();
  } else if (topicStr == "interrupt/alerts") {
    latestInterruptMessage = message;
  } else if (topicStr == "traffic/signal") {
    lastTrafficSignalTime = millis();  // update last receive time
    int colonIndex = message.indexOf(':');
    String content = message;
    if (colonIndex != -1) {
      content = message.substring(colonIndex + 1);
      content.trim();
    }
    int spaceIndex = content.indexOf(' ');
    if (spaceIndex > 0) {
      trafficColor = content.substring(0, spaceIndex); 
      trafficColor.trim();
      String tStr = content.substring(spaceIndex + 1);
      tStr.replace("s", "");
      trafficTime = tStr.toInt();
    }
  } else if (topicStr == "parking/status") {
    parkingSlots = message;
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("Car1Client")) {
      Serial.println("connected");
      mqttClient.subscribe("car1/control");
      mqttClient.subscribe("interrupt/alerts");
      mqttClient.subscribe("traffic/signal");
      mqttClient.subscribe("parking/status");
    } else {
      Serial.print("failed with state ");
      Serial.print(mqttClient.state());
      Serial.println(", retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void publishUltrasonicSpeedAndSpeedBreaker() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float filteredZ = getFilteredZ(a.acceleration.z);
  String roadMsg = detectRoadCondition(filteredZ);
  float distance = getDistanceCM();
  float speed = abs(g.gyro.z);

  String sensorPayload = "{";
  sensorPayload += "\"distance\":" + String(distance, 2) + ",";
  sensorPayload += "\"speed\":" + String(speed, 2);
  sensorPayload += "}";
  mqttClient.publish("car1/sensors", sensorPayload.c_str());

  if (roadMsg != "") {
    mqttClient.publish("car1/speedbreaker", roadMsg.c_str());
  } else {
    mqttClient.publish("car1/speedbreaker", "None");
  }
}

// ---- Twilio SMS Functions ----
void sendSMS(const char* toPhone, const char* message);
String base64Encode(const String &);
const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64Encode(const String &input) {
  String encoded = "";
  int i = 0;
  uint8_t char_array_3[3];
  uint8_t char_array_4[4];
  int input_len = input.length();
  int j = 0;

  while (input_len--) {
    char_array_3[i++] = input[j++];
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;
      for (i = 0; i < 4; i++) encoded += base64_chars[char_array_4[i]];
      i = 0;
    }
  }
  if (i) {
    for (int k = i; k < 3; k++) char_array_3[k] = '\0';
    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;
    for (int k = 0; k < i + 1; k++) encoded += base64_chars[char_array_4[k]];
    while ((i++ < 3)) encoded += '=';
  }
  return encoded;
}

void sendSMS(const char* toPhone, const char* message) {
  if (!secureClient.connect("api.twilio.com", 443)) {
    Serial.println("Connection to Twilio failed");
    return;
  }
  String auth = String(twilio_sid) + ":" + String(twilio_auth_token);
  String authHeader = "Authorization: Basic " + base64Encode(auth);
  String postData = "To=" + String(toPhone) + "&From=" + String(twilio_phone) + "&Body=" + String(message);

  secureClient.print(String("POST /2010-04-01/Accounts/") + twilio_sid + "/Messages.json HTTP/1.1\r\n" +
                     "Host: api.twilio.com\r\n" +
                     authHeader + "\r\n" +
                     "Content-Type: application/x-www-form-urlencoded\r\n" +
                     "Content-Length: " + postData.length() + "\r\n" +
                     "Connection: close\r\n\r\n" +
                     postData + "\r\n");

  while (secureClient.connected()) {
    String line = secureClient.readStringUntil('\n');
    if (line == "\r") break;
  }
  String response = secureClient.readString();
  Serial.println("Twilio response:");
  Serial.println(response);
}

// ---- Webserver (UI) ----
// Root page - motor controls and link to display
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Car1 Controller</title>
  <style>
    body { text-align: center; font-family: sans-serif; }
    button { width: 120px; height: 60px; font-size: 20px; margin: 10px; }
  </style>
  <script>
    function sendCmd(cmd) {
      fetch('/' + cmd);
    }
  </script>
</head>
<body>
  <h2>Car1 Controller</h2>
  <div>
    <button onclick="sendCmd('forward')">Forward</button>
  </div>
  <div>
    <button onclick="sendCmd('left')">Left</button>
    <button onclick="sendCmd('stop')">Stop</button>
    <button onclick="sendCmd('right')">Right</button>
  </div>
  <div>
    <button onclick="sendCmd('backward')">Backward</button>
  </div>
  <p><a href="/display">View Display</a></p>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// Display dashboard page
void handleDisplay() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Car1 Display</title>
<style>
body { margin:0; background:#808080; font-family:Arial,sans-serif; }
#container {
  display:grid; grid-template-columns: 200px 200px; grid-template-rows: 200px 200px;
  gap: 15px; padding:15px; box-sizing: border-box; justify-content:center;
}
.box {
  background:white; border-radius:10px; padding:10px; box-shadow:0 3px 8px rgba(0,0,0,0.15);
  overflow-y:auto; font-size:14px; max-width:200px; max-height:200px;
}
h2 { margin-top:0; color:#333; font-size:18px; text-align:center; }
#trafficSignal div, #interruptMessages div, #parkingSlots div, #sensorData pre {
  padding:6px 0; text-align:center; color:#555;
}
#sensorData pre {
  white-space: pre-wrap; word-wrap: break-word; font-size:13px; margin-bottom:5px;
  height: calc(100% - 36px);
}
#sensorData h3 { margin-top:5px; }
</style>
</head>
<body>
<div id="container">
  <div id="trafficSignal" class="box">
    <h2>Traffic Signal</h2>
    <div>Color: <span id="trafficColor">--</span></div>
    <div>Time Remaining: <span id="trafficTime">--</span> s</div>
  </div>
  <div id="interruptMessages" class="box">
    <h2>Interrupt Messages</h2>
    <div id="interruptMsg">No messages</div>
  </div>
  <div id="parkingSlots" class="box">
    <h2>Parking Slots</h2>
    <div id="parkingMsg">-- / --</div>
  </div>
  <div id="sensorData" class="box">
    <h2>Sensor Data</h2>
    <pre id="sensorPre">Loading...</pre>
    <h3>Speed Breaker Alert:</h3>
    <div id="speedBreakerMsg">None</div>
  </div>
</div>
<script>
let lastTime = 0;
let lastColor = '';

async function updateStatus() {
  const resp = await fetch('/status');
  const data = await resp.json();

  if(data.color !== lastColor){
    lastColor = data.color;
    lastTime = data.time;
  } else if(lastTime > 0){
    lastTime--;
  }

  document.getElementById('trafficColor').innerText = lastColor;
  document.getElementById('trafficTime').innerText = lastTime > 0 ? lastTime : 0;
  document.getElementById('interruptMsg').innerText = data.interrupt || "No messages";
  document.getElementById('parkingMsg').innerText = data.parking || "-- / --";
  document.getElementById('speedBreakerMsg').innerText = data.speedbreaker || "None";
  document.getElementById('sensorPre').innerText = JSON.stringify(data.sensor || {}, null, 2);
}
setInterval(updateStatus, 1000);
updateStatus();
</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}


void handleStatus() {
  long distance = getDistanceCM();
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float filteredZ = getFilteredZ(a.acceleration.z);
  String road = detectRoadCondition(filteredZ);
  float speed = abs(g.gyro.z);

  unsigned long now = millis();
  String displayTrafficColor = trafficColor;
  int displayTrafficTime = trafficTime;
  if (now - lastTrafficSignalTime > TRAFFIC_TIMEOUT_MS) {
    displayTrafficColor = "DISCONNECTED";
    displayTrafficTime = 0;
  }
  String sensorPayload = "{";
  sensorPayload += "\"distance\":" + String(distance) + ",";
  sensorPayload += "\"speed\":" + String(speed, 2) + ",";
  sensorPayload += "\"road\":\"" + road + "\"";
  sensorPayload += "}";
  String json = "{";
  json += "\"color\":\"" + displayTrafficColor + "\",";
  json += "\"time\":" + String(displayTrafficTime) + ",";
  json += "\"interrupt\":\"" + latestInterruptMessage + "\",";
  json += "\"parking\":\"" + parkingSlots + "\",";
  json += "\"speedbreaker\":\"" + speedBreakerMessage + "\",";
  json += "\"sensor\":" + sensorPayload;
  json += "}";

  server.send(200, "application/json", json);
}

// ---- SETUP & LOOP ----
void setup() {
  Serial.begin(115200);
  pinMode(rightMotorPin1, OUTPUT);
  pinMode(rightMotorPin2, OUTPUT);
  pinMode(leftMotorPin1, OUTPUT);
  pinMode(leftMotorPin2, OUTPUT);
  pinMode(enableRightMotor, OUTPUT);
  pinMode(enableLeftMotor, OUTPUT);

  ledcAttach(enableRightMotor, 1000, 8);
  ledcAttach(enableLeftMotor, 1000, 8);
  stopCar();
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip!");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);

  secureClient.setInsecure(); // For Twilio HTTPS

  server.on("/", handleRoot);
  server.on("/display", handleDisplay);
  server.on("/status", handleStatus);

  server.on("/forward", []() { forward(); server.send(200, "text/plain", "OK"); });
  server.on("/backward", []() { backward(); server.send(200, "text/plain", "OK"); });
  server.on("/left", []() { moveLeft(); server.send(200, "text/plain", "OK"); });
  server.on("/right", []() { moveRight(); server.send(200, "text/plain", "OK"); });
  server.on("/stop", []() { stopCar(); server.send(200, "text/plain", "OK"); });

  server.begin();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  server.handleClient();

  // Ultrasonic obstacle detection
  long distance = getDistanceCM();
  static bool prevObstacleDetected = false;
  obstacleDetected = (distance > 0 && distance < safeDistance);
  if (obstacleDetected && !prevObstacleDetected) {
    Serial.println("Obstacle detected! Stopping car.");
    stopCar();
  }
  prevObstacleDetected = obstacleDetected;

  // Accident detection and SMS alert
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  if (a.acceleration.z > 4.5) {
    if (!alertSent) {
      Serial.println("Tilt/accident detected! Sending SMS alerts.");
      for (int i = 0; i < 3; i++)
        sendSMS(alertNumbers[i], "Alert: Vehicle met with an accident!");
      alertSent = true;
    }
  } else {
    alertSent = false;
  }

  static unsigned long lastPublish = 0;
  unsigned long now = millis();
  if (now - lastPublish > 2000) {
    lastPublish = now;
    publishUltrasonicSpeedAndSpeedBreaker();
  }
}
