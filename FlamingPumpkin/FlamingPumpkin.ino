#include <ESP32Servo.h>

#ifdef B0
#undef B0
#endif
#define B0 21

#ifdef D5
#undef D5
#endif
#define D5 14

static const unsigned int FLAME_OFF = 75;
static const unsigned int FLAME_ON = 25;

Servo trigger;
volatile size_t flame_start_ms = 0;

void IRAM_ATTR flameOn() {
  digitalWrite(LED_BUILTIN, HIGH);
  if (!flame_start_ms) { flame_start_ms = millis(); }
}

void setup() {
  // Initialize the trigger
  trigger.write(FLAME_OFF);
  trigger.attach(D5);

  // Debug LED (mirrors flame)
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(LED_BUILTIN, OUTPUT);

  // Button Interrupt
  pinMode(B0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(B0), flameOn, FALLING);
}

void loop() {
  // Check for flame off condition
  delay(500);
  if ((millis() - flame_start_ms) > 500) {
    digitalWrite(LED_BUILTIN, LOW);
    trigger.write(FLAME_OFF);
    flame_start_ms = 0;
  } else {
    trigger.write(FLAME_ON);
  }
}
