#include <Arduino.h>
#line 1 "C:\\Users\\Andre\\AppData\\Local\\Arduino15\\web_server_ENG2000\\web_server_ENG2000.ino"
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

