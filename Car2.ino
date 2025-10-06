#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
// ================= WiFi and MQTT config =================
const char* ssid = "Jayy";
const char* password = "Jayaraj@231";
const char* mqtt_server = "broker.emqx.io";
const char* mqtt_user = "";
const char* mqtt_pass = "";
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);
// ================= Motor pins =================
int enableRightMotor = 22;
int rightMotorPin1 = 16;
int rightMotorPin2 = 17;
int enableLeftMotor = 23;
int leftMotorPin1 = 18;
int leftMotorPin2 = 19;
// ================= Ultrasonic pins =================
const int trigPin = 5;
const int echoPin = 4;
bool obstacleDetected = false;
// ================= Traffic signal tracking =================
unsigned long lastTrafficSignalTime = 0;
const unsigned long TRAFFIC_TIMEOUT_MS = 5000;  // 5 seconds timeout
String trafficColor = "RED";
int trafficTime = 0;
// ================= Variables from Car1 =================
String latestInterruptMessage = "No alerts yet.";
String parkingSlots = "--/--";
String sensorJson = "{}";
String speedBreakerMessage = "";
// ================= Motor functions =================
void stopCar() {
  digitalWrite(rightMotorPin1, LOW);
  digitalWrite(rightMotorPin2, LOW);
  digitalWrite(leftMotorPin1, LOW);
  digitalWrite(leftMotorPin2, LOW);
  ledcWrite(enableRightMotor, 0);
  ledcWrite(enableLeftMotor, 0);
}
void forward() {
  if(obstacleDetected) { stopCar(); return; }
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
  if(obstacleDetected) { stopCar(); return; }
  digitalWrite(rightMotorPin1, HIGH);
  digitalWrite(rightMotorPin2, LOW);
  digitalWrite(leftMotorPin1, LOW);
  digitalWrite(leftMotorPin2, HIGH);
  ledcWrite(enableRightMotor, 90);
  ledcWrite(enableLeftMotor, 90);
}
void moveRight() {
  if(obstacleDetected) { stopCar(); return; }
  digitalWrite(rightMotorPin1, LOW);
  digitalWrite(rightMotorPin2, HIGH);
  digitalWrite(leftMotorPin1, HIGH);
  digitalWrite(leftMotorPin2, LOW);
  ledcWrite(enableRightMotor, 90);
  ledcWrite(enableLeftMotor, 90);
}
// ================= Ultrasonic distance =================
long getDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  return duration * 0.034 / 2;
}
// ================= MQTT callback =================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String message = "";
  for (unsigned int i=0; i<length; i++) message += (char)payload[i];
  if (topicStr == "interrupt/alerts") {
    latestInterruptMessage = message;
  } 
  else if (topicStr == "traffic/signal") {
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
  }
  else if (topicStr == "parking/status") {
    parkingSlots = message;
  }
  else if (topicStr == "car1/sensors") {
    sensorJson = message;
  } 
  else if (topicStr == "car1/speedbreaker") {
    speedBreakerMessage = message;
  }
  else if (topicStr == "car2/control") {
    if (message == "forward") forward();
    else if (message == "backward") backward();
    else if (message == "left") moveLeft();
    else if (message == "right") moveRight();
    else if (message == "stop") stopCar();
  }
}
// ================= MQTT reconnect =================
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "Car2Client-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected!");
      mqttClient.subscribe("interrupt/alerts");
      mqttClient.subscribe("traffic/signal");
      mqttClient.subscribe("parking/status");
      mqttClient.subscribe("car1/sensors");
      mqttClient.subscribe("car1/speedbreaker");
      mqttClient.subscribe("car2/control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}
// ================= Publish IP =================
void publishIP() {
  String ip = WiFi.localIP().toString();
  Serial.print("ESP32 IP: ");
  Serial.println(ip);
  if (mqttClient.connected()) {
    mqttClient.publish("car2/ip", ip.c_str());
  }
}
// ================= Web pages =================
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Car 2 Control</title>";
  html += "<style>body{background:lightblue;text-align:center;font-family:sans-serif;}";
  html += "button{width:120px;height:60px;font-size:20px;margin:10px;";
  html += "background:#e0f7fa;border:none;border-radius:10px;box-shadow:0 3px 6px rgba(0,0,0,0.2);}</style>";
  html += "<script>function sendCmd(cmd){fetch('/'+cmd);}</script></head><body>";
  html += "<h2>Car 2 Control</h2>";
  html += "<div><button onclick=\"sendCmd('forward')\">Forward</button></div>";
  html += "<div><button onclick=\"sendCmd('left')\">Left</button>"
          "<button onclick=\"sendCmd('stop')\">Stop</button>"
          "<button onclick=\"sendCmd('right')\">Right</button></div>";
  html += "<div><button onclick=\"sendCmd('backward')\">Backward</button></div>";
  html += "<p><a href='/display'>View Dashboard</a></p></body></html>";
  server.send(200, "text/html", html);
}
void handleDisplay() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Car 2 Controller</title>
<style>
  body {margin:0;background:lightblue; font-family:Arial,sans-serif;}
  #container {
    display:grid; grid-template-columns:200px 200px; grid-template-rows:200px 200px;
    gap:15px; padding:15px; box-sizing:border-box; justify-content:center;
  }
  .box {
    background:white; border-radius:10px; padding:10px; box-shadow:0 3px 8px rgba(0,0,0,0.15);
    overflow-y:auto; font-size:14px; max-width:200px; max-height:200px;
  }
  h2 {margin-top:0; color:#333; font-size:18px; text-align:center;}
  #sensorData pre {
    white-space:pre-wrap; word-wrap:break-word; height:calc(100% - 36px);
    font-size:13px; margin-bottom:5px;
  }
  #trafficLights {
    display:flex; flex-direction:column; align-items:center; gap:10px; margin-top:10px;
  }
  .light {
    width:40px; height:40px; border-radius:50%; background:#444;
    box-shadow:0 0 5px rgba(0,0,0,0.4);
  }
  .active { box-shadow:0 0 20px 5px rgba(0,0,0,0.5); }
  .red.active { background:red; }
  .yellow.active { background:yellow; }
  .green.active { background:limegreen; }
</style>
</head>
<body>
<div id="container">
  <div id="trafficSignal" class="box">
    <h2>Traffic Signal</h2>
    <div id="trafficLights">
      <div id="redLight" class="light red"></div>
      <div id="yellowLight" class="light yellow"></div>
      <div id="greenLight" class="light green"></div>
    </div>
    <div style="margin-top:10px; text-align:center;">
      Time Remaining: <span id="trafficTime">--</span> s
    </div>
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
    <pre id="sensorPre"></pre>
    <h3>Speed Breaker Alert:</h3>
    <div id="speedBreakerMsg">None</div>
    <h3>Obstacle:</h3>
    <div id="obstacleMsg">Clear</div>
  </div>
</div>
<script>
let lastTime = 0;
let lastColor = '';
async function updateStatus() {
  const resp = await fetch('/status');
  const data = await resp.json();
  // Traffic light handling with disconnect detection
  if(data.color !== lastColor){
    lastColor = data.color;
    lastTime = data.time;
  } else if(lastTime > 0){
    lastTime--;
  }
  document.getElementById('trafficTime').innerText = lastTime > 0 ? lastTime : 0;
  document.getElementById('redLight').classList.remove('active');
  document.getElementById('yellowLight').classList.remove('active');
  document.getElementById('greenLight').classList.remove('active');
  if(lastColor === "RED") document.getElementById('redLight').classList.add('active');
  else if(lastColor === "YELLOW") document.getElementById('yellowLight').classList.add('active');
  else if(lastColor === "GREEN") document.getElementById('greenLight').classList.add('active');
  // Other messages
  document.getElementById('interruptMsg').innerText = data.interrupt || "No messages";
  document.getElementById('parkingMsg').innerText = data.parking || "-- / --";
  document.getElementById('speedBreakerMsg').innerText = data.speedbreaker || "None";
  document.getElementById('obstacleMsg').innerText = data.obstacle || "Clear";
  if(Object.keys(data.sensor||{}).length === 0){
    document.getElementById('sensorPre').innerText = "No Data";
  } else {
    document.getElementById('sensorPre').innerText = JSON.stringify(data.sensor, null, 2);
  }
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
  unsigned long now = millis();
  String displayTrafficColor = trafficColor;
  int displayTrafficTime = trafficTime;
  if (now - lastTrafficSignalTime > TRAFFIC_TIMEOUT_MS) {
    displayTrafficColor = "DISCONNECTED";
    displayTrafficTime = 0;
  }
  String json = "{";
  json += "\"color\":\"" + displayTrafficColor + "\",";
  json += "\"time\":" + String(displayTrafficTime) + ",";
  json += "\"interrupt\":\"" + latestInterruptMessage + "\",";
  json += "\"parking\":\"" + parkingSlots + "\",";
  json += "\"speedbreaker\":\"" + speedBreakerMessage + "\",";
  json += "\"obstacle\":\"" + String(obstacleDetected ? "STOPPED (<20cm)" : "Clear") + "\",";
  json += "\"sensor\":" + sensorJson;
  json += "}";
  server.send(200, "application/json", json);
}
// ================= Setup =================
void setup() {
  Serial.begin(115200);
  pinMode(rightMotorPin1, OUTPUT);
  pinMode(rightMotorPin2, OUTPUT);
  pinMode(leftMotorPin1, OUTPUT);
  pinMode(leftMotorPin2, OUTPUT);
  pinMode(enableRightMotor, OUTPUT);
  pinMode(enableLeftMotor, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  ledcAttach(enableRightMotor, 1000, 8);
  ledcAttach(enableLeftMotor, 1000, 8);
  stopCar();
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
  reconnectMQTT();
  publishIP();
  server.on("/", handleRoot);
  server.on("/display", handleDisplay);
  server.on("/status", handleStatus);
  server.on("/forward", [](){ forward(); server.send(200, "text/plain", "OK"); });
  server.on("/backward", [](){ backward(); server.send(200, "text/plain", "OK"); });
  server.on("/left", [](){ moveLeft(); server.send(200, "text/plain", "OK"); });
  server.on("/right", [](){ moveRight(); server.send(200, "text/plain", "OK"); });
  server.on("/stop", [](){ stopCar(); server.send(200, "text/plain", "OK"); });
  server.begin();
}
// ================= Loop =================
void loop() {
  if(!mqttClient.connected()) {
    reconnectMQTT();
    publishIP();
  }
  mqttClient.loop();
  server.handleClient();
  // Ultrasonic check
  long dist = getDistanceCM();
  if(dist > 0 && dist < 20) {
    if(!obstacleDetected) {
      Serial.println("Obstacle detected! Stopping car.");
      obstacleDetected = true;
      stopCar();
    }
  } else {
    obstacleDetected = false;
  }
}
