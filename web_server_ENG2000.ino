#include <ezButton.h>
#include <WiFi.h>
#include <WebServer.h>

//network credentials
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

// --- Pin definitions ---
 //front ultrasonic
const int trigFront = 12;
const int echoFront = 13;
// back ultrasonic
const int trigBack = 14;
const int echoBack = 27;

// motor
const int motorUpPin = 32;
const int motorDownPin = 33;

// LED
const int safeLight = 26;

//top Switches
ezButton topLimit(2);
//Bottom Switches
ezButton bottomLimit(15);

// --- Thresholds ---
const int boatApproachDist = 50;   // cm
const int boatClearDist = 200;     // cm

// --- State flags ---
bool bridgeUp = false;

// Web server port
WebServer server(80);

// --- Function Prototypes ---
long ultrasonicDistance(int trigPin, int echoPin);
void liftBridge();
void lowerBridge();
void boatApproaching();
void boatCleared();

void setup() {
  // initialize serial communications at 9600 bp  s:
  Serial.begin(115200);

  pinMode(trigFront, OUTPUT);
  pinMode(echoFront, INPUT);
  pinMode(trigBack, OUTPUT);
  pinMode(echoBack, INPUT);
  pinMode(motorUpPin, OUTPUT);
  pinMode(motorDownPin, OUTPUT);
  pinMode(safeLight, OUTPUT);

  topLimit.setDebounceTime(50);
  bottomLimit.setDebounceTime(50);

  digitalWrite(motorUpPin, LOW);
  digitalWrite(motorDownPin, LOW);
  digitalWrite(safeLight, LOW);

  // Start Wi-Fi AP
  WiFi.softAP(ssid, password);
  Serial.println("WiFi Access Point started!");
  Serial.println(WiFi.softAPIP());
  
  // Web server routes
  server.on("/", handleRoot);
  server.on("/lift", handleLift);
  server.on("/lower", handleLower);
  // server.on("/auto", handleAutoToggle);

  server.begin();
  Serial.println("Web server started!");
}

void loop(){
  topLimit.loop();
  bottomLimit.loop();
  server.handleClient();

  // calculate the distance if there is a boat in front of the bridge
  long frontDist = ultrasonicDistance(trigFront, echoFront);
  // calculate the distance if there is a boat in the back of the bridge
  long backDist  = ultrasonicDistance(trigBack, echoBack);

  Serial.print("Front: "); Serial.print(frontDist);
  Serial.print("Back: "); Serial.println(backDist);

  if (frontDist < boatApproachDist && !bridgeUp) {
    boatApproaching();
  }

  if (backDist > boatClearDist && bridgeUp) {
    boatCleared();
  }

  delay(200);
}

// ===== Web Pages =====
void handleRoot() {
  String html = "<html><body style='text-align:center;font-family:sans-serif'>";
  html += "<h2>🌉 ESP32 Bascule Bridge Control</h2>";
  html += bridgeUp ? "<p><b>Status:</b> Bridge is <span style='color:green'>UP</span></p>"
                   : "<p><b>Status:</b> Bridge is <span style='color:red'>DOWN</span></p>";
  html += "<p><a href='/lift'><button>LIFT</button></a>";
  html += "<a href='/lower'><button>LOWER</button></a></p>";
  // html += autoMode ? "<p><a href='/auto'><button>Disable Auto Mode</button></a></p>"
  //                  : "<p><a href='/auto'><button>Enable Auto Mode</button></a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

long ultrasonicDistance(int trigpin, int echopin){
  digitalWrite(trigpin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigpin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigpin, LOW);

  long duration = pulseIn(echopin, HIGH);
  int distance = duration * 0.034 / 2;
  return distance;
}

//lfiting bridge
void liftBridge() {
  digitalWrite(motorDownPin, LOW);
  digitalWrite(motorUpPin, HIGH);
  while (!topLimit.isPressed()) {
    topLimit.loop();
  }
  digitalWrite(motorUpPin, LOW);
  bridgeUp = true;
  digitalWrite(safeLight, HIGH);
}

//lowering bridge
void lowerBridge() {
  digitalWrite(motorDownPin, HIGH);
  digitalWrite(motorUpPin, LOW);
  while (!bottomLimit.isPressed()) {
    bottomLimit.loop();
  }
  digitalWrite(motorUpPin, LOW);
  bridgeUp = false;
  digitalWrite(safeLight, LOW);
}

//for web server to handle lift and lower bridge manually
void handleLift() {
  liftBridge();
  server.sendHeader("Location", "/");
  server.send(303);
  }
void handleLower() {
  lowerBridge();
  server.sendHeader("Location", "/");
  server.send(303);
  }

//when boat is approaching
void boatApproaching() {
  Serial.println("Boat approaching detected! Lifting bridge...");
  digitalWrite(safeLight, LOW);
  liftBridge();
  Serial.println("Bridge lifted. Safe light ON.");
}

//when boat is clear
void boatCleared() {
  Serial.println("Boat cleared! Lowering bridge...");
  digitalWrite(safeLight, LOW);
  lowerBridge();
  Serial.println("Bridge lowered. Safe light ON.");
}