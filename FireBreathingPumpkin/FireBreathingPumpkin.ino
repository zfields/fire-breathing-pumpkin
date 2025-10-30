#include <Notecard.h>
#include <SparkFun_Qwiic_Ultrasonic_Arduino_Library.h>

#ifdef ARDUINO_ARCH_ESP32
#  include <ESP32Servo.h>
#  ifdef D5
#  undef D5
#  endif
#  define D5 14
#  ifdef D10
#  undef D10
#  endif
#  define D10 33
#else
#  include <Servo.h>
#endif

static const unsigned int BURN_MS = 750;
static const int FLAME_OFF_DEGREES = 75;
static const int FLAME_ON_DEGREES = 25;

volatile size_t flame_request_ms = 0;

QwiicUltrasonic safety;
Servo trigger;

bool burn = false;
bool safety_engaged = false;
bool safety_attached = false;

#ifdef ARDUINO_ARCH_ESP32
void IRAM_ATTR fireInTheHole() {
#else
void fireInTheHole() {
#endif
  digitalWrite(LED_BUILTIN, HIGH);
  if (!flame_request_ms) { flame_request_ms = millis(); }
}

void extinguishFlame (void) {
  digitalWrite(LED_BUILTIN, LOW);
  trigger.write(FLAME_OFF_DEGREES);
  burn = false;
}

void igniteFlame (void) {
  burn = true;
  trigger.write(FLAME_ON_DEGREES);
}

void setup() {
  // Initialize the trigger
  trigger.attach(D5);
  trigger.write(FLAME_OFF_DEGREES);

  // Debug LED (mirrors flame)
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize distance sensor
  safety_attached = safety.begin(kQwiicUltrasonicDefaultAddress);
  if (!safety_attached) {
    // Blink Warning
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
  }

  // Button Interrupt
  pinMode(D10, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(D10), fireInTheHole, RISING);
}

void loop() {
  // Check line of fire
  uint16_t mm = 0xFFFF;
  if (safety_attached) {
    if (safety.triggerAndRead(mm) || mm < 1000) {
      if (burn) {
        extinguishFlame();
      }
      safety_engaged = true;
      return; // `continue`
    } else if (safety_engaged && flame_request_ms) {
      delay(1000);  // Allow victim to clear line of fire
      flame_request_ms = millis();  // Advance pending flame request
    }
    safety_engaged = false;  // All clear
  }

  // Check flame request
  if (flame_request_ms) {
    if ((millis() - flame_request_ms) <= BURN_MS) {
      if (!burn) {
        igniteFlame();
      }
    } else {
      flame_request_ms = 0;
      extinguishFlame();
    }
  }
}
