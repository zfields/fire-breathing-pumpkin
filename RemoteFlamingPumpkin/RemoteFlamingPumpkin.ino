#include <ESP32Servo.h>
#include <Notecard.h>

#ifdef B0
#undef B0
#endif
#define B0 21

#ifdef D5
#undef D5
#endif
#define D5 14

#ifdef D6
#undef D6
#endif
#define D6 32

#define serialDebug Serial
#define productUID "<com.your_company.your_product>"

static const unsigned int FLAME_OFF = 75;
static const unsigned int FLAME_ON = 30;

Servo trigger;
volatile size_t flame_start_ms = 0;
volatile bool notehub_request = false;

Notecard notecard;
 
void IRAM_ATTR fireInTheHole() {
  digitalWrite(LED_BUILTIN, HIGH);
  if (!flame_start_ms) { flame_start_ms = millis(); }
}

void IRAM_ATTR notehubRequest() {
  notehub_request = true;
  digitalWrite(LED_BUILTIN, HIGH);
  if (!flame_start_ms) { flame_start_ms = millis(); }
}

void setup() {
  // Initialize the actuator
  trigger.write(FLAME_OFF);
  trigger.attach(D5);

  // Attach Button Interrupt
  pinMode(B0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(B0), fireInTheHole, FALLING);

  // Debug LED (mirrors flame)
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize Debug Output
  serialDebug.begin(115200);
  while (!serialDebug) {
    ; // wait for serial port to connect. Needed for native USB
  }

  // Initialize Notecard
  notecard.begin();
  notecard.setDebugOutputStream(serialDebug);

  // Configure Notecard
  if (J *req = notecard.newRequest("hub.set")) {
    JAddStringToObject(req, "product", productUID);
    JAddStringToObject(req, "mode", "continuous");
    JAddBoolToObject(req, "sync", true);
    if (!notecard.sendRequest(req)) {
      notecard.logDebug("FATAL: Failed to configure Notecard!\n");
      while(1);
    }
  }

  // Arm ATTN Interrupt
  if (J *req = NoteNewRequest("card.attn")) {
    JAddStringToObject(req, "mode", "arm,files");
    if (J *files = JAddArrayToObject(req, "files")) {
      JAddItemToArray(files, JCreateString("flame.qi"));
      if (!notecard.sendRequest(req)) {
        notecard.logDebug("ERROR: Failed to arm ATTN interrupt!\n");
      }
    }
  }

  // Attach Notecard Interrupt
  pinMode(D6, INPUT);
  attachInterrupt(digitalPinToInterrupt(D6), notehubRequest, RISING);
}

void loop() {
  // Check for flame off condition
  delay(500);

  // Process flame request
  // NOTE: Servos cannot be actuated in ISRs
  if (flame_start_ms) {
    if ((millis() - flame_start_ms) > 500) {
      trigger.write(FLAME_OFF);
      digitalWrite(LED_BUILTIN, LOW);

      // Special handling for Notehub
      if (notehub_request) {
        notehub_request = false;
        // Delete flame request
        if (J *req = NoteNewRequest("note.get")) {
          JAddStringToObject(req, "file", "flame.qi");
          JAddBoolToObject(req, "delete", true);
      
          if (!notecard.sendRequest(req)) {
            notecard.logDebug("ERROR: Failed to delete Note!\n");
          }
        }
  
        // Rearm ATTN Interrupt
        if (J *req = NoteNewRequest("card.attn")) {
          JAddStringToObject(req, "mode", "arm,files");
          if (J *files = JAddArrayToObject(req, "files")) {
            JAddItemToArray(files, JCreateString("flame.qi"));
            if (!notecard.sendRequest(req)) {
              notecard.logDebug("ERROR: Failed to rearm ATTN interrupt!\n");
            }
          }
        }
      }

      flame_start_ms = 0;
    } else {
      trigger.write(FLAME_ON);
    }
  }
}
