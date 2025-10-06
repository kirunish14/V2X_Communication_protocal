#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
 
// WiFi credentials
const char* ssid = "Jayy";
const char* password = "Jayaraj@231";

// MQTT broker info
const char* mqtt_server = "broker.emqx.io";

WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

enum LightColor { RED, GREEN, YELLOW };
LightColor currentColor = RED;
unsigned long lastChange = 0;
int duration = 0;

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Traffic Light</title>
<style>
body {
  font-family: Arial, sans-serif;
  background:#f0f0f0;
  display:flex;
  justify-content:center;
  align-items:center;
  height:100vh;
}
#trafficBox {
  background:#fff;
  padding:30px;
  border-radius:25px;
  box-shadow:0 10px 30px rgba(0,0,0,0.3);
  width:450px;
  display:flex;
  align-items:center;
}
#lights {
  display:flex;
  flex-direction:column;
  margin-right:30px;
}
.light {
  width:60px;
  height:60px;
  border-radius:50%;
  background:#ccc;
  filter: brightness(50%);
  transition: all 0.3s ease;
  margin:12px 0;
}
.red { background:red; }
.yellow { background:yellow; }
.green { background:green; }
.active {
  filter: brightness(200%);
  box-shadow:0 0 35px #000;
}
.colorName {
  margin-left:15px;
  font-size:20px;
  font-weight:bold;
  visibility:hidden;
}
#status {
  display:flex;
  flex-direction:column;
  justify-content:center;
}
#timerBox {
  margin-left:auto;
  background:#eee;
  padding:15px 20px;
  border-radius:15px;
  font-weight:bold;
  font-size:20px;
  text-align:center;
  min-width:80px;
}
.row {
  display:flex;
  align-items:center;
}
</style>
</head>
<body>
<div id="trafficBox">
  <div id="lights">
    <div class="row">
      <div id="red" class="light red"></div>
      <div id="redName" class="colorName">RED</div>
    </div>
    <div class="row">
      <div id="yellow" class="light yellow"></div>
      <div id="yellowName" class="colorName">YELLOW</div>
    </div>
    <div class="row">
      <div id="green" class="light green"></div>
      <div id="greenName" class="colorName">GREEN</div>
    </div>
  </div>
  <div id="status">
    <div id="timerBox">--</div>
  </div>
</div>
<script>
function updateStatus() {
  fetch('/status').then(resp => resp.json()).then(data => {
    ['red','yellow','green'].forEach(c=>{
      document.getElementById(c).classList.remove('active');
      document.getElementById(c+'Name').style.visibility = 'hidden';
    });
    document.getElementById(data.color.toLowerCase()).classList.add('active');
    document.getElementById(data.color.toLowerCase()+'Name').style.visibility = 'visible';
    document.getElementById('timerBox').innerText = data.time + 's';
  });
}
setInterval(updateStatus, 1000);
updateStatus();
</script>
</body>
</html>
)rawliteral";

String colorToString(LightColor color) {
  switch(color) {
    case RED: return "RED";
    case GREEN: return "GREEN";
    case YELLOW: return "YELLOW";
  }
  return "UNKNOWN";
}

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleStatus() {
  unsigned long now = millis();
  int elapsed = (now - lastChange) / 1000;
  int remaining = duration - elapsed;
  String json = "{\"color\":\"" + colorToString(currentColor) + "\", \"time\":" + String(remaining) + "}";
  server.send(200, "application/json", json);
}

void nextLight() {
  switch(currentColor) {
    case RED: currentColor = GREEN; duration = 30; break;
    case GREEN: currentColor = YELLOW; duration = 5; break;
    case YELLOW: currentColor = RED; duration = 30; break;
  }
  lastChange = millis();

  // Publish traffic signal status over MQTT
  String message = "TRAFFIC:" + colorToString(currentColor) + " " + String(duration) + "s";
  mqttClient.publish("traffic/signal", message.c_str());
}

bool reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
  return false;
}


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi:");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(mqtt_server, 1883);

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.begin();

  currentColor = RED;
  duration = 30;
  lastChange = millis();

  // Publish initial status
  String message = "TRAFFIC:" + colorToString(currentColor) + " " + String(duration) + "s";
  mqttClient.publish("traffic/signal", message.c_str());
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  server.handleClient();

  if ((millis() - lastChange)/1000 >= duration) {
    nextLight();
  }
}
