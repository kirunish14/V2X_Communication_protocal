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

String latestInterruptMessage = "No interrupt messages.";

// HTML page for input form
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Interrupt Message</title>
</head>
<body style="font-family:Arial; text-align:center; background:#f8f8f8;">
  <h2>Send Emergency Interrupt Message</h2>
  <form action="/send" method="POST">
    <input type="text" name="message" placeholder="Enter message" required style="padding:10px; width:60%;">
    <br><br>
    <input type="submit" value="Send" style="padding:10px 20px; font-size:16px;">
  </form>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSend() {
  if (server.hasArg("message")) {
    latestInterruptMessage = server.arg("message");
    // Prefix with EMERGENCY: for recognition
    String fullMessage = "EMERGENCY:" + latestInterruptMessage;

    // Publish the interrupt message via MQTT
    mqttClient.publish("interrupt/alerts", fullMessage.c_str());

    server.send(200, "text/html", "<h3>Message Sent Successfully</h3><a href='/'>Back</a>");
    Serial.println("Sent: " + fullMessage);
  } else {
    server.send(400, "text/html", "<h3>Error: No message provided</h3><a href='/'>Back</a>");
  }
}

void handleGetMessage() {
  server.send(200, "text/plain", latestInterruptMessage);
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("InterruptSenderClient")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi:");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(mqtt_server, 1883);

  server.on("/", handleRoot);
  server.on("/send", HTTP_POST, handleSend);
  server.on("/message", HTTP_GET, handleGetMessage);
  server.begin();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  server.handleClient();
}
