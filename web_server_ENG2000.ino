#include <WiFi.h>
#include <WebServer.h>

// --- Network credentials ---
const char* ssid = "EEFFEsp32";
const char* password = "Bridges";

WebServer server(80);

// --- Pin definitions ---
const int trigFront = 12;
const int echoFront = 13;
const int trigBack = 14;
const int echoBack = 27;
const int motorUpPin = 32;
const int motorDownPin = 33;
const int safeLight = 26;
const int topLimitPin = 16;
const int bottomLimitPin = 17;

// --- Thresholds ---
int boatApproachDist = 20;  // cm
int boatClearDist = 40;     // cm
int debounceDelay = 50;     // ms
int stateBufferTime = 10000; // ms

// --- Global Variables ---
bool bridgeUp = false;
bool manualOverride = false;
int buttonState[2] = { LOW, LOW };
int lastButtonState[2] = { LOW, LOW };
float ultrasonicFrontDist = 0;
float ultrasonicBackDist = 0;
bool boatApproachDirForward = false;
unsigned long lastDebounceTime[2] = { 0, 0 };
unsigned long timer = 0;
unsigned long lastUpdate = 0;

// --- PWM Channels ---
const int pwmChannelUp = 0;
const int pwmChannelDown = 1;
const int pwmFreq = 5000;
const int pwmResolution = 8;

// --- State Machine ---
enum State { IDLE, BOAT_APPROACHING, OPENING, WAITING, CLOSING, MANUAL };
State state = IDLE;

// --- Button indices ---
const int topLimitIndex = 0;
const int bottomLimitIndex = 1;

// --- Setup ---
void setup() {
  Serial.begin(9600);

  pinMode(trigFront, OUTPUT);
  pinMode(echoFront, INPUT);
  pinMode(trigBack, OUTPUT);
  pinMode(echoBack, INPUT);
  pinMode(safeLight, OUTPUT);
  pinMode(topLimitPin, INPUT_PULLUP);
  pinMode(bottomLimitPin, INPUT_PULLUP);

  // Setup PWM
  ledcSetup(pwmChannelUp, pwmFreq, pwmResolution);
  ledcSetup(pwmChannelDown, pwmFreq, pwmResolution);
  ledcAttachPin(motorUpPin, pwmChannelUp);
  ledcAttachPin(motorDownPin, pwmChannelDown);

  // Initialize outputs
  ledcWrite(pwmChannelUp, 0);
  ledcWrite(pwmChannelDown, 0);
  digitalWrite(safeLight, LOW);

  // Start Wi-Fi AP
  WiFi.softAP(ssid, password);
  Serial.println("WiFi Access Point started!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Web server routes
  server.on("/", handleRoot);
  server.on("/open", replyOpen);
  server.on("/close", replyClose);
  server.on("/stop", replyStop);
  server.on("/standby", replyStandby);

  server.begin();
  Serial.println("Web server started!");
}

// --- Loop ---
void loop() {
  server.handleClient();

  // Update ultrasonic distances
  float checkDistance = ultrasonicDistance(trigFront, echoFront);
  if (checkDistance != 0) ultrasonicFrontDist = checkDistance;
  checkDistance = ultrasonicDistance(trigBack, echoBack);
  if (checkDistance != 0) ultrasonicBackDist = checkDistance;

  // Update every 50 ms
  if (millis() - lastUpdate > 50) {
    lastUpdate = millis();

    if (!manualOverride) {  // Automatic control
      switch (state) {
        case IDLE:
          if (ultrasonicFrontDist < boatApproachDist) {
            state = BOAT_APPROACHING;
            boatApproachDirForward = true;
            digitalWrite(safeLight, HIGH);
            timer = millis() + stateBufferTime;
          } else if (ultrasonicBackDist < boatApproachDist) {
            state = BOAT_APPROACHING;
            boatApproachDirForward = false;
            digitalWrite(safeLight, HIGH);
            timer = millis() + stateBufferTime;
          }
          break;

        case BOAT_APPROACHING:
          digitalWrite(safeLight, (millis() / 500) % 2);
          if (millis() > timer) {
            state = OPENING;
            digitalWrite(safeLight, LOW);
          }
          break;

        case OPENING:
          if (digitalRead(topLimitPin) == LOW) {
            state = WAITING;
            timer = millis() + stateBufferTime;
            motorStop();
          } else motorOpen();
          break;

        case WAITING:
          digitalWrite(safeLight, HIGH);
          if (millis() < timer) break;
          if (boatApproachDirForward) {
            if (ultrasonicBackDist > boatClearDist && ultrasonicFrontDist > boatApproachDist)
              state = CLOSING;
          } else {
            if (ultrasonicFrontDist > boatClearDist && ultrasonicBackDist > boatApproachDist)
              state = CLOSING;
          }
          break;

        case CLOSING:
          digitalWrite(safeLight, (millis() / 1000) % 2);
          if (digitalRead(bottomLimitPin) == LOW) {
            state = IDLE;
            digitalWrite(safeLight, HIGH);
            motorStop();
          } else motorClose();
          break;

        case MANUAL:
          motorStop();
          break;
      }
    }
  }

  debug(true);
}

// --- Ultrasonic ---
float ultrasonicDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(trigPin, LOW);
  float duration = pulseIn(echoPin, HIGH, 15000);
  return duration * 0.034 / 2;
}

// --- Motor Control ---
void motorOpen() {
  if (digitalRead(topLimitPin) == HIGH) {
    ledcWrite(pwmChannelUp, 255);
    ledcWrite(pwmChannelDown, 0);
    bridgeUp = true;
  } else motorStop();
}

void motorClose() {
  if (digitalRead(bottomLimitPin) == HIGH) {
    ledcWrite(pwmChannelUp, 0);
    ledcWrite(pwmChannelDown, 255);
    bridgeUp = false;
  } else motorStop();
}

void motorStop() {
  ledcWrite(pwmChannelUp, 0);
  ledcWrite(pwmChannelDown, 0);
}

// --- Web Handlers ---
void handleRoot() {
  String html = "<html><body style='text-align:center;font-family:sans-serif'>";
  html += "<h2>🌉 ESP32 Bascule Bridge Control</h2>";
  html += bridgeUp ? "<p><b>Status:</b> Bridge is <span style='color:green'>UP</span></p>"
                   : "<p><b>Status:</b> Bridge is <span style='color:red'>DOWN</span></p>";
  html += manualOverride ? "<p><b>Mode:</b> Manual Control</p>" 
                         : "<p><b>Mode:</b> Automatic Control</p>";
  html += "<p><a href='/open'><button>LIFT</button></a> ";
  html += "<a href='/close'><button>LOWER</button></a></p>";
  html += "<p><a href='/standby'><button>STANDBY</button></a> ";
  html += "<a href='/stop'><button>STOP</button></a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void replyOpen() {
  manualOverride = true;
  state = OPENING;
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

void replyClose() {
  manualOverride = true;
  state = CLOSING;
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

void replyStop() {
  manualOverride = true;
  state = MANUAL;
  motorStop();
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

void replyStandby() {
  manualOverride = false;
  state = IDLE;
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

// --- Debug ---
void debug(bool enabled) {
  if (!enabled) return;
  Serial.print("State: ");
  Serial.print((int)state);
  Serial.print("  USFront: ");
  Serial.print(ultrasonicFrontDist);
  Serial.print(" cm  USBack: ");
  Serial.print(ultrasonicBackDist);
  Serial.print(" cm  TopLimit: ");
  Serial.print(digitalRead(topLimitPin));
  Serial.print("  BottomLimit: ");
  Serial.println(digitalRead(bottomLimitPin));
}
