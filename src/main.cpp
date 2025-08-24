#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// ----------- Pin Definitions -----------
#define TOUCH_OPEN     4    // Capacitive touch sensor (open - BROWN)
#define TOUCH_CLOSE    2    // Capacitive touch sensor (close - RED)
#define TOUCH_STOP     15   // Capacitive touch sensor (stop - BLACK)

#define OPEN_RELAY     32  // Relay to open door (pulse)
#define CLOSE_RELAY    33  // Relay to close door (hold)
#define STOP_RELAY     25  // Relay to stop door

#define SENSOR1_PIN    12  // Magnetic reed sensor 1
#define SENSOR2_PIN    14  // Magnetic reed sensor 2

// ----------- Wi-Fi Credentials -----------
const char* ssid = "1227";
const char* password = "OrIon$@42";

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

// Function to get door state as a string
String getDoorState() {
  if (digitalRead(SENSOR1_PIN) == LOW) return "Closed";   // S1 closed = door closed
  if (digitalRead(SENSOR2_PIN) == LOW) return "Open";     // S2 closed = door open
  return "Moving";
}

// Helper: Open Door (only if S2 open)
void openDoor() {
  if (digitalRead(SENSOR2_PIN) == HIGH && !openRelayActive) {
    digitalWrite(OPEN_RELAY, HIGH);
    openRelayActive = true;
    openRelayTimer = millis();
    digitalWrite(STOP_RELAY, LOW); // Ensure stop released
  }
}

// Helper: Close Door (only if S1 open)
void closeDoor() {
  if (digitalRead(SENSOR1_PIN) == HIGH && !closeRelayActive) {
    digitalWrite(CLOSE_RELAY, HIGH);
    closeRelayActive = true;
    closeRelayTimer = millis();
    digitalWrite(STOP_RELAY, LOW);
  }
}

// Helper: Stop Door (IMMEDIATE relay open)
void stopDoor() {
  openRelayActive = false;
  closeRelayActive = false;
  digitalWrite(CLOSE_RELAY, LOW);  // Open close relay immediately
  digitalWrite(OPEN_RELAY, LOW);   // Ensure open relay releases
  digitalWrite(STOP_RELAY, HIGH);    // Activate stop relay
  delay(buttonTime);
  digitalWrite(STOP_RELAY, LOW);   // Release stop relay
}

// ------------------ HTML Web Page ---------------------
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Garage Controller</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
body { font-family: Arial; text-align: center; }
button { font-size: 2em; margin: 10px; padding: 20px 40px; }
.status { font-weight: bold; font-size: 2em; margin: 20px;}
</style>
</head>
<body>
<h1>Garage Door Control</h1>
<div class="status" id="doorState">Loading...</div>
<button onclick="sendCmd('open')">Open</button>
<button onclick="sendCmd('close')">Close</button>
<button onclick="sendCmd('stop')">Stop</button>
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
  WiFi.mode(WIFI_STA);  // ap for access point, sta for client
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print(WiFi.SSID());

  // Web Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
    request->send_P(200, "text/html", webpage); 
  });
  server.on("/api/open", HTTP_POST, [](AsyncWebServerRequest *request){
    openDoor();
    request->send(200, "text/plain", "OK");
  });
  server.on("/api/close", HTTP_POST, [](AsyncWebServerRequest *request){
    closeDoor();
    request->send(200, "text/plain", "OK");
  });
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    stopDoor();
    request->send(200, "text/plain", "OK");
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
  if (touchRead(TOUCH_OPEN) < 30)   openDoor();
  if (touchRead(TOUCH_CLOSE) < 30)  closeDoor();
  if (touchRead(TOUCH_STOP) < 30)   stopDoor();

  // Timed Relay Control
  if (openRelayActive && millis() - openRelayTimer > buttonTime) {
    digitalWrite(OPEN_RELAY, LOW); // Release relay
    openRelayActive = false;
  }
  if (closeRelayActive && millis() - closeRelayTimer > closeTime) {
    digitalWrite(CLOSE_RELAY, LOW); // Release relay
    closeRelayActive = false;
  }
}
