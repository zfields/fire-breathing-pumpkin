#include <Notecard.h>
#include <SparkFun_Qwiic_Ultrasonic_Arduino_Library.h>

#ifdef ARDUINO_ARCH_ESP32
#  include <ESP32Servo.h>
#  ifdef D5
#  undef D5
#  endif
#  define D5 14
#  ifdef D9
#  undef D9
#  endif
#  define D9 15
#  ifdef D10
#  undef D10
#  endif
#  define D10 33
#else
#  include <Servo.h>
#endif

// This is the unique Product Identifier for your device
#ifndef PRODUCT_UID
#define PRODUCT_UID "" // "com.my-company.my-name:my-project"
#pragma message "PRODUCT_UID is not defined in this example. Please ensure your Notecard has a product identifier set before running this example or define it in code here. More details at https://dev.blues.io/tools-and-sdks/samples/product-uid"
#endif

#define myProductID PRODUCT_UID

static const unsigned int BURN_MS = 750;
static const int FLAME_OFF_DEGREES = 75;
static const int FLAME_ON_DEGREES = 25;

volatile size_t flame_request_ms = 0;
volatile bool notehub_request = false;

Notecard notecard;
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

#ifdef ARDUINO_ARCH_ESP32
void IRAM_ATTR notehubRequest() {
#else
void notehubRequest() {
#endif
  notehub_request = true;
  fireInTheHole();
}

void configureNotecard (void) {
  // Initialize Notecard
  notecard.begin();

  // Configure Notecard
  if (J *req = notecard.newRequest("hub.set")) {
    JAddStringToObject(req, "product", myProductID);
    JAddStringToObject(req, "sn", "Fire Breathing Pumpkin");
    JAddStringToObject(req, "mode", "continuous");
    JAddBoolToObject(req, "sync", true);
    notecard.sendRequest(req);
  }

  // Arm ATTN Interrupt
  if (J *req = NoteNewRequest("card.attn")) {
    JAddStringToObject(req, "mode", "arm,files");
    if (J *files = JAddArrayToObject(req, "files")) {
      JAddItemToArray(files, JCreateString("flame.qi"));
      notecard.sendRequest(req);
    }
  }

  // Attach Notecard Interrupt
  pinMode(D9, INPUT);
  attachInterrupt(digitalPinToInterrupt(D9), notehubRequest, RISING);
}

void resetNotecard (void) {
  notehub_request = false;

  // Delete flame request(s)
  bool notes_available = false;
  do {
    if (J *req = NoteNewRequest("note.get")) {
      JAddStringToObject(req, "file", "flame.qi");
      JAddBoolToObject(req, "delete", true);
      notes_available = notecard.sendRequest(req);
    }
  } while (notes_available);

  // Rearm ATTN Interrupt
  if (J *req = NoteNewRequest("card.attn")) {
    JAddStringToObject(req, "mode", "rearm");
    notecard.sendRequest(req);
  }
}

void extinguishFlame (void) {
  digitalWrite(LED_BUILTIN, LOW);
  trigger.write(FLAME_OFF_DEGREES);
  burn = false;

  // Special handling for Notehub
  if (notehub_request) {
    notehub_request = false;
    resetNotecard();
  }
}

void igniteFlame (void) {
  burn = true;
  trigger.write(FLAME_ON_DEGREES);
}

void setup() {
  Serial.begin(115200);
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

  // Configure Notecard
  configureNotecard();
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
