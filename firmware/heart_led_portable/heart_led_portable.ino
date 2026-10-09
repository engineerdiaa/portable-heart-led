// IG @diaawastaken LED heart Arduino code
// Please follow for future projects
// Portable version: new pin map + LiPo battery check on A6

// --- PIN DEFINITIONS ---
const int buttonPin = 2;   // Interrupt pin for the button (other leg to GND)
const int topLed = 12;     // Top center (dip between the lobes)
const int bottomLed = 4;   // Bottom tip

// Symmetrical sides (7 LEDs each), level 1 (top) -> level 7 (bottom)
const int leftSide[]  = {11, 10, 9, 8, 7, 6, 5};
const int rightSide[] = {13, A0, A1, A2, A3, A4, A5};

// Full clockwise rotation (front view): Top -> Right side down -> Bottom -> Left side up
const int clockwisePath[] = {12, 13, A0, A1, A2, A3, A4, A5, 4, 5, 6, 7, 8, 9, 10, 11};

// Array of all 16 LEDs for bulk on/off operations
const int allLeds[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A0, A1, A2, A3, A4, A5};
const int numTotalLeds = 16;

// --- BATTERY CHECK SETTINGS ---
// A6 -> 10k resistor -> boost VIN+ (after the switch)
const int batteryPin = A6;
const float SUPPLY_V = 5.14;          // Measured voltage on the Nano's 5V pin (ADC reference)
const float LOW_BATTERY_V = 3.30;     // Shut down below this (under load)
const float NO_BATTERY_V = 2.50;      // Below this = running from USB, skip the check
const byte LOW_READINGS_NEEDED = 5;   // Consecutive low readings before shutdown
const unsigned long CHECK_INTERVAL = 1000; // ms between battery checks

#define DEBUG_BATTERY 0  // Set to 1 to print battery voltage in the Serial Monitor (9600 baud)

// --- STATE VARIABLES ---
volatile int stage = 1;
volatile bool stageChanged = false;
volatile unsigned long lastButtonPress = 0;

// Animation speeds (milliseconds) - SPLIT FOR INDEPENDENT CONTROL
const int stage2Speed = 130;    // Tuned for the top-to-bottom drop
const int stage3Speed = 250;    // Slower for the binary alternating flicker
const int clockwiseSpeed = 90;
const int blinkSpeed = 600;

void setup() {
  // Initialize all LEDs as outputs and turn them off
  for (int i = 0; i < numTotalLeds; i++) {
    pinMode(allLeds[i], OUTPUT);
    digitalWrite(allLeds[i], LOW);
  }

  // Initialize button with internal pull-up
  pinMode(buttonPin, INPUT_PULLUP);

  // Attach interrupt to Pin 2 for instant button response
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, FALLING);

#if DEBUG_BATTERY
  Serial.begin(9600);
#endif
}

void loop() {
  stageChanged = false; // Reset the flag before entering a stage

  switch (stage) {
    case 1: stage1(); break;
    case 2: stage2(); break;
    case 3: stage3(); break;
    case 4: stage4(); break;
    case 5: stage5(); break;
    case 6: stage6(); break;
  }
}

// --- INTERRUPT SERVICE ROUTINE (Debounce & State Change) ---
void buttonISR() {
  unsigned long currentMillis = millis();
  // 200ms debounce window
  if (currentMillis - lastButtonPress > 200) {
    stage++;
    if (stage > 6) {
      stage = 1;
    }
    lastButtonPress = currentMillis;
    stageChanged = true; // Signal to break out of current animation loop
  }
}

// --- BATTERY FUNCTIONS ---
float readBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < 4; i++) {     // Average 4 samples to reduce noise
    sum += analogRead(batteryPin);
  }
  return (sum / 4.0) * SUPPLY_V / 1023.0;
}

// Called constantly from smartDelay(); only actually measures once per CHECK_INTERVAL
void checkBattery() {
  static unsigned long lastCheck = 0;
  static byte lowCount = 0;

  if (millis() - lastCheck < CHECK_INTERVAL) return;
  lastCheck = millis();

  float vbat = readBatteryVoltage();

#if DEBUG_BATTERY
  Serial.print("Battery: ");
  Serial.print(vbat, 2);
  Serial.println(" V");
#endif

  if (vbat < NO_BATTERY_V) {        // No battery on A6 (e.g. powered from USB)
    lowCount = 0;
    return;
  }

  if (vbat < LOW_BATTERY_V) {
    lowCount++;
    if (lowCount >= LOW_READINGS_NEEDED) {
      lowBatteryShutdown();          // Never returns
    }
  } else {
    lowCount = 0;                    // Brief dips don't count
  }
}

// Turns everything off and flashes the bottom LED as a "charge me" warning.
// Stays here until the switch is turned off. The Nano and boost still draw
// a small current, so switch off and charge soon.
void lowBatteryShutdown() {
  detachInterrupt(digitalPinToInterrupt(buttonPin));
  turnAllOff();
  while (true) {
    digitalWrite(bottomLed, HIGH);
    delay(50);
    digitalWrite(bottomLed, LOW);
    delay(2950);
  }
}

// --- SMART DELAY ---
// Allows animations to pause, but breaks out IMMEDIATELY if the button is pressed.
// Also runs the battery check in the background.
void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    checkBattery();
    if (stageChanged) return;
  }
}

// --- HELPER FUNCTIONS ---
void turnAllOff() {
  for (int i = 0; i < numTotalLeds; i++) {
    digitalWrite(allLeds[i], LOW);
  }
}

void turnAllOn() {
  for (int i = 0; i < numTotalLeds; i++) {
    digitalWrite(allLeds[i], HIGH);
  }
}

// Maps a step (0 to 8) to the corresponding horizontal "level" of LEDs
void lightLevel(int level) {
  if (level == 0) {
    digitalWrite(topLed, HIGH);              // Level 0: Top
  }
  else if (level >= 1 && level <= 7) {
    int idx = level - 1;
    digitalWrite(leftSide[idx], HIGH);       // Level 1-7: Symmetrical sides
    digitalWrite(rightSide[idx], HIGH);
  }
  else if (level == 8) {
    digitalWrite(bottomLed, HIGH);           // Level 8: Bottom
  }
}

// --- STAGE LOGIC ---

// Stage 1: All OFF
void stage1() {
  turnAllOff();
  while (!stageChanged) {
    smartDelay(10); // Idle, waiting for interrupt (battery check keeps running)
  }
}

// Stage 2: Top to bottom pairs
void stage2() {
  while (!stageChanged) {
    for (int i = 0; i <= 8; i++) { // 9 total vertical levels (0 to 8)
      turnAllOff();
      lightLevel(i);
      smartDelay(stage2Speed);
      if (stageChanged) return;
    }
  }
}

// Stage 3: Alternating Binary Flow
void stage3() {
  while (!stageChanged) {
    // Frame 1: Turn ON all "Even" levels (0, 2, 4, 6, 8)
    turnAllOff();
    for (int i = 0; i <= 8; i += 2) {
      lightLevel(i);
    }
    smartDelay(stage3Speed);
    if (stageChanged) return;

    // Frame 2: Turn ON all "Odd" levels (1, 3, 5, 7)
    turnAllOff();
    for (int i = 1; i <= 8; i += 2) {
      lightLevel(i);
    }
    smartDelay(stage3Speed);
    if (stageChanged) return;
  }
}

// Stage 4: Chaser going clockwise around the heart
void stage4() {
  while (!stageChanged) {
    for (int i = 0; i < numTotalLeds; i++) {
      turnAllOff();
      digitalWrite(clockwisePath[i], HIGH);
      smartDelay(clockwiseSpeed);
      if (stageChanged) return;
    }
  }
}

// Stage 5: All ON / All OFF
void stage5() {
  while (!stageChanged) {
    turnAllOn();
    smartDelay(blinkSpeed);
    if (stageChanged) return;
    turnAllOff();
    smartDelay(blinkSpeed);
    if (stageChanged) return;
  }
}

// Stage 6: All ON steady
void stage6() {
  turnAllOn();
  while (!stageChanged) {
    smartDelay(10); // Battery check keeps running
  }
}
