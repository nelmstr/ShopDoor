#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "secrets.h"  // WiFi credentials
#include <HTTPClient.h> // Send alerts to ntfy service

// ----------- Pin Definitions -----------
#define TOUCH_OPEN     4    // Capacitive touch sensor (open - BROWN)
#define TOUCH_CLOSE    2    // Capacitive touch sensor (close - RED)
#define TOUCH_STOP     15   // Capacitive touch sensor (stop - BLACK)

#define OPEN_RELAY     32  // Relay to open door (pulse)
#define CLOSE_RELAY    33  // Relay to close door (hold)
#define STOP_RELAY     25  // Relay to stop door

#define SENSOR1_PIN    12  // Magnetic reed sensor 1
#define SENSOR2_PIN    14  // Magnetic reed sensor 2


// ----------- Globals -----------
AsyncWebServer server(80);

bool doorIsClosed = false;
bool openRelayActive = false;
bool closeRelayActive = false;

// ----------- Relay operation timers -----------
unsigned long openRelayTimer = 0;
unsigned long closeRelayTimer = 0;

// Duration to hold the close relay (in milliseconds)
const unsigned long closeTime = 15*1000; // Adjust as needed for your door
const unsigned long buttonTime = 250; // Open relay pulse duration

// Time to check how long the door has been open before sending alert
const unsigned long doorOpenAlertTime = 30*60*1000; // 30 minutes

// WiFi reconnect timer
unsigned long previousMillis = 0;
const long interval = 10000; // 20 seconds

// Function to send alert via ntfy service
void sendAlert(const String& message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(NTFY_SERVER); // Replace with your ntfy topic or define in secrets.h
    http.addHeader("Title", "Shop Door Alert");
    int httpResponseCode = http.POST(message);
    if (httpResponseCode > 0) {
      Serial.println("Alert sent successfully");
    } else {
      Serial.print("Error sending alert: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected. Cannot send alert.");
  }
}

// Function to get door state as a string
String getDoorState() {
  if (digitalRead(SENSOR1_PIN) == LOW) return "Closed";   // S1 closed = door closed
  // if (digitalRead(SENSOR2_PIN) == LOW) return "Open";     // S2 closed = door open
  return "Moving/Open";
}

// Helper: Open Door (only if S2 open)
void openDoor() {
  if (digitalRead(SENSOR2_PIN) == HIGH && !openRelayActive) {
    Serial.println("Opening door...");
    digitalWrite(OPEN_RELAY, HIGH);
    openRelayActive = true;
    openRelayTimer = millis();
    digitalWrite(STOP_RELAY, LOW); // Ensure stop released
    sendAlert("Info: Shop door is opening.");
  }
}

// Helper: Close Door (only if S1 open)
void closeDoor() {
  if (digitalRead(SENSOR1_PIN) == HIGH && !closeRelayActive) {
    Serial.println("Closing door...");
    digitalWrite(CLOSE_RELAY, HIGH);
    closeRelayActive = true;
    closeRelayTimer = millis();
    digitalWrite(STOP_RELAY, LOW);
    sendAlert("Info: Shop door is closing.");
  }
}

// Helper: Stop Door (IMMEDIATE relay open)
void stopDoor() {
  Serial.println("Stopping door...");
  openRelayActive = false;
  closeRelayActive = false;
  digitalWrite(CLOSE_RELAY, LOW);  // Open close relay immediately
  digitalWrite(OPEN_RELAY, LOW);   // Ensure open relay releases
  digitalWrite(STOP_RELAY, HIGH);    // Activate stop relay
  delay(buttonTime);
  digitalWrite(STOP_RELAY, LOW);   // Release stop relay
  sendAlert("Info: Shop door is stopped.");
}

// ------------------ HTML Web Page ---------------------
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Shop Door Controller</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
body { font-family: Arial; text-align: center; }
button { font-size: 2em; margin: 10px; padding: 20px 40px; }
.status { font-weight: bold; font-size: 2em; margin: 20px;}
</style>
</head>
<body>
<h1>Garage Door Control</h1>
<div class="status" id="doorState">Loading...</div><br>
<button onclick="sendCmd('open')">Open</button><br>
<button onclick="sendCmd('close')">Close</button><br>
<button onclick="sendCmd('stop')">Stop</button><br>
<script>
function sendCmd(cmd) {
  fetch('/api/' + cmd, { method:'POST' });
}
function updateState() {
  fetch('/api/state').then(x => x.json()).then(json => {
    document.getElementById('doorState').innerText = json.state;
  });
}
setInterval(updateState, 1000);
window.onload = updateState;
</script>
</body>
</html>
)rawliteral";


// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);

  // Pins
  pinMode(OPEN_RELAY, OUTPUT); digitalWrite(OPEN_RELAY, LOW);
  pinMode(CLOSE_RELAY, OUTPUT); digitalWrite(CLOSE_RELAY, LOW);
  pinMode(STOP_RELAY, OUTPUT); digitalWrite(STOP_RELAY, LOW);

  pinMode(SENSOR1_PIN, INPUT_PULLUP);
  pinMode(SENSOR2_PIN, INPUT_PULLUP);



  // Wi-Fi
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);  // ap for access point, sta for client
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("Connected to WiFi");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.SSID());

  // Web Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
    request->send(200, "text/html", webpage); 
  });
  server.on("/api/open", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("API Open");
    openDoor();
    request->send(200, "text/plain", "Opening Door");
  });
  server.on("/api/close", HTTP_POST, [](AsyncWebServerRequest *request){
    closeDoor();
    request->send(200, "text/plain", "Closing Door");
  });
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    stopDoor();
    request->send(200, "text/plain", "Door Stopped");
  });
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"state\":\"" + getDoorState() + "\"}";
    request->send(200, "application/json", json);
  });

  server.begin();
}

// ------------------- MAIN LOOP -------------------
void loop() {
  // Capacitive buttons (simple threshold: adjust as needed)
    // Serial.println(TOUCH_OPEN);
  // if (touchRead(TOUCH_OPEN) < 50) {
  //   Serial.print("Touch Open: ");
  //   Serial.println(TOUCH_OPEN);
    // openDoor();
  // } 
  // if (touchRead(TOUCH_CLOSE) < 30)  closeDoor();
  // if (touchRead(TOUCH_STOP) < 30)   stopDoor();

  // Timed Relay Control
  if (openRelayActive && millis() - openRelayTimer > buttonTime) {
    digitalWrite(OPEN_RELAY, LOW); // Release relay
    openRelayActive = false;
  }
  if (closeRelayActive && millis() - closeRelayTimer > closeTime) {
    digitalWrite(CLOSE_RELAY, LOW); // Release relay
    closeRelayActive = false;
  }

    unsigned long currentMillis = millis();
  if (WiFi.status() != WL_CONNECTED && currentMillis - previousMillis >= interval) {
    Serial.println("WiFi connection lost. Reconnecting...");
    ESP.restart();
    // WiFi.disconnect();
    // WiFi.reconnect();
    previousMillis = currentMillis;
  }

  // Door Open Alert
  if (digitalRead(SENSOR1_PIN) == HIGH) { // Door is not closed
    if (!doorIsClosed) {
      static unsigned long doorOpenTimer = millis();
      if (millis() - doorOpenTimer > doorOpenAlertTime) {
        sendAlert("Alert: Garage door has been open for over 30 minutes!");
        doorIsClosed = true; // Prevent multiple alerts
      }
    }
  }
}
