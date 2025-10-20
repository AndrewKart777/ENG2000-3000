#include <ezButton.h>

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

// --- Function Prototypes ---
long readUltrasonicDistance(int trigPin, int echoPin);
void liftBridge();
void lowerBridge();
void boatApproaching();
void boatCleared();

void setup() {
  // initialize serial communications at 9600 bp  s:
  Serial.begin(115200);

  pinMode(motorUpPin, OUTPUT);
  pinMode(motorDownPin, OUTPUT);
  pinMode(safeLight, OUTPUT);

  topLimit.setDebounceTime(50);
  bottomLimit.setDebounceTime(50);

  digitalWrite(motorUpPin, LOW);
  digitalWrite(motorDownPin, LOW);
  digitalWrite(safeLight, LOW);
}

void loop(){
  topLimit.loop();
  bottomLimit.loop();

  long frontDist = ultrasonicDistance(trigFront, echoFront);
  long backDist  = ultrasonicDistance(trigBack, echoBack);

  Serial.print("Front: "); Serial.print(frontDist);
  Serial.print("  Back: "); Serial.println(backDist);

  if (frontDist < boatApproachDist && !bridgeUp) {
    boatApproaching();
  }

  if (backDist > boatClearDist && bridgeUp) {
    boatCleared();
  }

  delay(200);
}

void ultrasonicDistance(int trigpin, int echopin){
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2;
}

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

void lowerBridge() {
  digitalWrite(motorDownPin, HIGH);
  digitalWrite(motorUpPin, LOW);
  while (!bottomLimit.isPressed()) {
    bottomLimit.loop();
  }
  digitalWrite(motorUpPin, HIGH);
  bridgeUp = FALSE;
  digitalWrite(safeLight, LOW);
}

void boatApproaching() {
  Serial.println("Boat approaching detected! Lifting bridge...");
  digitalWrite(safeLight, LOW);
  liftBridge();
  Serial.println("Bridge lifted. Safe light ON.");
}

void boatCleared() {
  Serial.println("Boat cleared! Lifting bridge...");
  digitalWrite(safeLight, LOW);
  lowerBridge();
  Serial.println("Bridge lowered. Safe light ON.");
}