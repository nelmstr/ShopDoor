#include <WiFi.h>
#include <WebServer.h>

// Pin Definitions
#define RELAY_OPEN     32   // Relay to open garage (momentary, 250 ms)
#define RELAY_CLOSE    33   // Relay to close garage (30 sec)
#define RELAY_STOP     25   // Relay to stop door

#define TOUCH_OPEN     4    // Capacitive touch sensor (open - BROWN)
#define TOUCH_CLOSE    2    // Capacitive touch sensor (close - RED)
#define TOUCH_STOP     15   // Capacitive touch sensor (stop - BLACK)

#define SENSOR_CLOSED  12   // Reed sensor 1 (closed position)
#define SENSOR_OPEN    13   // Reed sensor 2 (open position)

// WiFi credentials
const char* ssid = "1224";
const char* password = "OrIon$@42";

WebServer server(80);

// State variables
bool doorClosed = false;
bool doorOpen = false;
volatile bool stopRequested = false;

// Duration to keep the close relay active (e.g., 30 seconds)
const unsigned long closeTime = 15000;

// Helper functions
void updateSensors() {
  doorClosed = digitalRead(SENSOR_CLOSED) == LOW; // assuming LOW = sensor triggered (door closed)
  doorOpen   = digitalRead(SENSOR_OPEN) == LOW;   // assuming LOW = sensor triggered (door open)
}

// Relay actions
void openDoor() {
  updateSensors();
  if (!doorOpen && !stopRequested) {
    digitalWrite(RELAY_OPEN, HIGH);
    Serial.print("Opening door...");
    delay(250);
    digitalWrite(RELAY_OPEN, LOW);
  }
}

void closeDoor() {
  updateSensors();
  if (!doorClosed && !stopRequested) {
    digitalWrite(RELAY_CLOSE, HIGH);
    Serial.print("Closing door...");
    unsigned long startTime = millis();
    while (millis() - startTime < closeTime) {
      if (stopRequested) break;
      // delay(closeTime/100); // check every 300 ms
    }
    digitalWrite(RELAY_CLOSE, LOW);
  }
  stopRequested = false; // reset stop flag after closing attempt
}

void stopDoor() {
  stopRequested = true;
    Serial.print("Stop Requested!");
  digitalWrite(RELAY_STOP, HIGH);
  digitalWrite(RELAY_CLOSE, LOW); // release close relay if active
  digitalWrite(RELAY_OPEN, LOW);  // release open relay just in case
  delay(500);
  digitalWrite(RELAY_STOP, LOW);
}

// Web page handlers
void handleRoot() {
  updateSensors();
  String html = "<html><body>\n";
  html += "<h2>Garage Door Control</h2>\n";
  html += "<form method='get' action='/open'><button>Open</button></form>\n";
  html += "<form method='get' action='/close'><button>Close</button></form>\n";
  html += "<form method='get' action='/stop'><button>Stop</button></form>\n";
  html += "<p>";
  if (doorClosed)
    html += "Door Status: <b>Closed</b><br>\n";
  else if (doorOpen)
    html += "Door Status: <b>Open</b><br>\n";
  else
    html += "Door Status: <b>Unknown/Moving</b><br>\n";
  html += "</p></body></html>\n";
  server.send(200, "text/html", html);
}

void handleOpen() { openDoor(); handleRoot(); }
void handleClose() { closeDoor(); handleRoot(); }
void handleStop() { stopDoor(); handleRoot(); }

void setup() {
  Serial.begin(115200);

  // Pin modes
  pinMode(RELAY_OPEN, OUTPUT); digitalWrite(RELAY_OPEN, LOW); Serial.print("Relay Open initialized");
  pinMode(RELAY_CLOSE, OUTPUT); digitalWrite(RELAY_CLOSE, LOW); Serial.print("Relay Close initialized");
  pinMode(RELAY_STOP, OUTPUT);  digitalWrite(RELAY_STOP, LOW);  Serial.print("Relay Stop initialized");
  
  pinMode(SENSOR_CLOSED, INPUT_PULLUP);
  pinMode(SENSOR_OPEN, INPUT_PULLUP);

  // Connect WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
  }
  Serial.println("\nWiFi connected, IP address: " + WiFi.localIP().toString());  

  // Web server routes
  server.on("/", handleRoot);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.on("/stop", handleStop);
  server.begin();
}

void loop() {
  server.handleClient();

  // Check stop button first to override other operations
  if (touchRead(TOUCH_STOP) < 30) {
    stopDoor();
  } else {
    if (touchRead(TOUCH_OPEN) < 30) openDoor();
    if (touchRead(TOUCH_CLOSE) < 30) closeDoor();
  }
}
