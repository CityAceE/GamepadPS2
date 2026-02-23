/*
===============================================================================
 NES to PS/2 Sinclair Joystick Adapter v1.0
===============================================================================

 Author : Stanislav Yudin (CityAceE)
 GitHub : https://github.com/CityAceE
 Date   : 2026-02-22

-------------------------------------------------------------------------------
 Description
-------------------------------------------------------------------------------
 Allows an NES controller to emulate Sinclair-compatible joysticks or
 emulator controls via the PS/2 keyboard protocol.

 Designed for ATmega328P-based boards (Arduino Nano / Uno)
 and compatible with the TTGO VGA32 ZX Spectrum emulator.

 Uses PS2dev library:
 https://github.com/harbaum/ps2dev

-------------------------------------------------------------------------------
 Features
-------------------------------------------------------------------------------
 • 4 selectable layouts:
     - QAOPM
     - Left Sinclair
     - Right Sinclair
     - Emulator Control (Cursor-style)

 • Full PAUSE/Break support (correct 8-byte sequence, no break code)
 • Extended key support (0xE0-prefixed scan codes)
 • Swap A/B functionality with EEPROM persistence
 • Safe layout switching (automatic key release)
 • Non-blocking configuration mode
 • SELECT functions as both:
     - normal key
     - configuration modifier

-------------------------------------------------------------------------------
 Configuration Mode
-------------------------------------------------------------------------------
 Hold SELECT to enter configuration mode.

   D-Pad:
     UP      → Cursor / Emulator layout
     DOWN    → QAOPM
     LEFT    → Left Sinclair
     RIGHT   → Right Sinclair

   Button A → Force SwapAB OFF (standard mapping)
   Button B → Force SwapAB ON  (swapped mapping)

===============================================================================
*/

// ============================================================
// USER CONFIGURATION
// ============================================================
#define DEBUG      false    // true = debug output to Serial
#define NO_KEY     0x00     // Button disabled
#define PAUSE_CODE 0xFE     // Pause key (special 8-byte sequence)

// ============================================================
// PINS
// ============================================================
#define PS2_DATA 2
#define PS2_CLOCK 3
#define NES_LATCH 8
#define NES_CLOCK A5
#define NES_DATA  A4
#define EEPROM_ADDR 0

// ============================================================
// CONSTANTS
// ============================================================
#define LAYOUT_COUNT 4
#define EVENT_INTERVAL_US 2500
#define QUEUE_SIZE 64
#define BUTTON_COUNT 8

// ============================================================
// PS/2 SCAN CODES - Normal keys (no prefix)
// ============================================================
#define PS2_A       0x1C
#define PS2_B       0x32
#define PS2_Q       0x15
#define PS2_M       0x3A
#define PS2_N       0x31
#define PS2_O       0x44
#define PS2_P       0x4D
#define PS2_0       0x45
#define PS2_1       0x16
#define PS2_2       0x1E
#define PS2_3       0x26
#define PS2_4       0x25
#define PS2_5       0x2E
#define PS2_6       0x36
#define PS2_7       0x3D
#define PS2_8       0x3E
#define PS2_9       0x46
#define PS2_ESC     0x76
#define PS2_F1      0x05
#define PS2_ENTER   0x5A

// Extended keys (require 0xE0 prefix)
#define PS2_EXT_LEFT  0x6B
#define PS2_EXT_RIGHT 0x74
#define PS2_EXT_UP    0x75
#define PS2_EXT_DOWN  0x72

// ============================================================
// LIBRARIES & GLOBALS
// ============================================================
#include <EEPROM.h>
#include <ps2dev.h>

PS2dev keyboard(PS2_CLOCK, PS2_DATA);

uint8_t queue[QUEUE_SIZE];
volatile uint8_t qHead = 0;
volatile uint8_t qTail = 0;
unsigned long lastSendTime = 0;

// Button state tracking
bool buttonState[BUTTON_COUNT] = {false};

// Configuration
uint8_t layout = 2;
bool swapAB = false;
bool selectMode = false;
uint8_t lastNesState = 0xFF;

// NES bit mapping: 0:A, 1:B, 2:Select, 3:Start, 4:Up, 5:Down, 6:Left, 7:Right
const uint8_t nesBitIndex[BUTTON_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7};

// Button names for DEBUG
const char* buttonNames[BUTTON_COUNT] = {
  "A ", "B ", "SELECT ", "START ", "UP ", "DOWN ", "LEFT ", "RIGHT "
};

// ============================================================
// LAYOUT DEFINITIONS
// Button order: [0]A, [1]B, [2]Select, [3]Start, [4]Up, [5]Down, [6]Left, [7]Right
// ============================================================

// Layout 0 — QAOPM + PAUSE on START + ESC on SELECT
const uint8_t layout_QAOPM[BUTTON_COUNT] = {
  PS2_M,        // A     -> M (FIRE)
  PS2_Q,        // B     -> Q (Up alternative)
  PS2_ESC,      // SELECT -> ESC
  PAUSE_CODE,   // START  -> Pause
  PS2_Q,        // UP     -> Q
  PS2_A,        // DOWN   -> A
  PS2_O,        // LEFT   -> O
  PS2_P         // RIGHT  -> P
};
const bool layout_QAOPM_ext[BUTTON_COUNT] = {
  false, false, false, false, false, false, false, false
};

// Layout 1 — Left Sinclair (1-5) + PAUSE on START + ESC on SELECT
const uint8_t layout_LeftSinclair[BUTTON_COUNT] = {
  PS2_5,        // A     -> 5 (FIRE)
  PS2_4,        // B     -> 4 (Up alternative)
  PS2_ESC,      // SELECT -> ESC
  PAUSE_CODE,   // START  -> Pause
  PS2_4,        // UP     -> 4
  PS2_3,        // DOWN   -> 3
  PS2_1,        // LEFT   -> 1
  PS2_2         // RIGHT  -> 2
};
const bool layout_LeftSinclair_ext[BUTTON_COUNT] = {
  false, false, false, false, false, false, false, false
};

// Layout 2 — Right Sinclair (6-0) + PAUSE on START + ESC on SELECT
const uint8_t layout_RightSinclair[BUTTON_COUNT] = {
  PS2_0,        // A     -> 0 (FIRE)
  PS2_9,        // B     -> 9 (Up alternative)
  PS2_ESC,      // SELECT -> ESC
  PAUSE_CODE,   // START  -> Pause
  PS2_9,        // UP     -> 9
  PS2_8,        // DOWN   -> 8
  PS2_6,        // LEFT   -> 6
  PS2_7         // RIGHT  -> 7
};
const bool layout_RightSinclair_ext[BUTTON_COUNT] = {
  false, false, false, false, false, false, false, false
};

// Layout 3 — Emulator Control + F1 on SELECT
const uint8_t layout_Emulator[BUTTON_COUNT] = {
  PS2_2,            // A     -> 2
  PS2_3,            // B     -> 3
  PS2_F1,           // SELECT -> F1
  PS2_ENTER,        // START  -> Enter
  PS2_EXT_UP,       // UP     -> Up Arrow (EXT)
  PS2_EXT_DOWN,     // DOWN   -> Down Arrow (EXT)
  PS2_EXT_LEFT,     // LEFT   -> Left Arrow (EXT)
  PS2_EXT_RIGHT     // RIGHT  -> Right Arrow (EXT)
};
const bool layout_Emulator_ext[BUTTON_COUNT] = {
  false, false, false, false,  // A, B, SELECT, START
  true,  true,  true,  true   // UP, DOWN, LEFT, RIGHT (Extended)
};

// Layout arrays
const uint8_t* layouts[LAYOUT_COUNT] = {
  layout_QAOPM,
  layout_LeftSinclair,
  layout_RightSinclair,
  layout_Emulator
};

const bool* extFlags[LAYOUT_COUNT] = {
  layout_QAOPM_ext,
  layout_LeftSinclair_ext,
  layout_RightSinclair_ext,
  layout_Emulator_ext
};

const char* layoutNames[LAYOUT_COUNT] = {
  "QAOPM", "Left Sinclair", "Right Sinclair", "Emulator Control"
};

// ============================================================
// QUEUE FUNCTIONS
// ============================================================
void enqueue(uint8_t b) {
  uint8_t next = (qHead + 1) % QUEUE_SIZE;
  if (next != qTail) {
    queue[qHead] = b;
    qHead = next;
  }
}

// PAUSE sequence: 8 bytes, no break code
const uint8_t PAUSE_SEQ[8] = {0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xF0, 0x77};

void enqueuePause() {
  uint8_t free = (qTail > qHead) ? (qTail - qHead - 1) : (QUEUE_SIZE - (qHead - qTail) - 1);
  if (free >= 8) {
    for (int i = 0; i < 8; i++) {
      uint8_t n = (qHead + 1) % QUEUE_SIZE;
      queue[qHead] = PAUSE_SEQ[i];
      qHead = n;
    }
  }
}

void safeWrite(uint8_t b) {
  while (digitalRead(PS2_CLOCK) == LOW);
  while (digitalRead(PS2_DATA) == LOW);
  keyboard.write(b);
  delayMicroseconds(150);
}

void processQueue() {
  if (qHead == qTail) return;
  if (micros() - lastSendTime < EVENT_INTERVAL_US) return;
  safeWrite(queue[qTail]);
  qTail = (qTail + 1) % QUEUE_SIZE;
  lastSendTime = micros();
}

// ============================================================
// NES READ
// ============================================================
uint8_t readNES() {
  uint8_t state = 0;
  digitalWrite(NES_LATCH, HIGH);
  delayMicroseconds(12);
  digitalWrite(NES_LATCH, LOW);
  for (int i = 0; i < 8; i++) {
    if (digitalRead(NES_DATA)) state |= (1 << i);
    digitalWrite(NES_CLOCK, LOW);
    delayMicroseconds(6);
    digitalWrite(NES_CLOCK, HIGH);
    delayMicroseconds(6);
  }
  return state;
}

// ============================================================
// PS/2 SEND FUNCTIONS
// ============================================================
void sendMake(uint8_t code, bool ext) {
  if (code == NO_KEY) return;
  
  if (code == PAUSE_CODE) {
    enqueuePause();
    return;
  }
  
  if (ext) enqueue(0xE0);
  enqueue(code);
}

void sendBreak(uint8_t code, bool ext) {
  if (code == NO_KEY) return;
  if (code == PAUSE_CODE) return;  // Pause has no break code
  
  if (ext) enqueue(0xE0);
  enqueue(0xF0);
  enqueue(code);
}

void releaseAll(const uint8_t* keys, const bool* ext) {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    if (buttonState[i]) {
      sendBreak(keys[i], ext[i]);
      buttonState[i] = false;
    }
  }
}

// ============================================================
// EEPROM FUNCTIONS
// ============================================================
// Byte structure: [1:0]=Layout, [2]=SwapAB, [7:3]=Reserved
void saveConfig() {
  uint8_t configByte = layout | (swapAB << 2);
  EEPROM.write(EEPROM_ADDR, configByte);
}

void loadConfig() {
  uint8_t configByte = EEPROM.read(EEPROM_ADDR);
  uint8_t loadedLayout = configByte & 0x03;
  if (loadedLayout < LAYOUT_COUNT) {
    layout = loadedLayout;
    swapAB = (configByte >> 2) & 0x01;
  } else {
    layout = 2;
    swapAB = false;
    saveConfig();
  }
}

// ============================================================
// DEBUG FUNCTIONS
// ============================================================
#if DEBUG
const char* getKeyName(uint8_t c) {
  if (c == NO_KEY) return "NONE";
  if (c == PAUSE_CODE) return "PAUSE";
  switch (c) {
    case PS2_Q: return "Q";      case PS2_A: return "A";
    case PS2_O: return "O";      case PS2_P: return "P";
    case PS2_M: return "M";      case PS2_N: return "N";
    case PS2_ESC: return "ESC";  case PS2_F1: return "F1";
    case PS2_1: return "1";      case PS2_2: return "2";
    case PS2_3: return "3";      case PS2_4: return "4";
    case PS2_5: return "5";      case PS2_6: return "6";
    case PS2_7: return "7";      case PS2_8: return "8";
    case PS2_9: return "9";      case PS2_0: return "0";
    case PS2_ENTER: return "ENTER";
    case PS2_EXT_LEFT: return "LEFT";  case PS2_EXT_RIGHT: return "RIGHT";
    case PS2_EXT_DOWN: return "DOWN";  case PS2_EXT_UP: return "UP";
    default: return "UNK";
  }
}

void printConfig() {
  Serial.print("Layout: ");
  Serial.print(layoutNames[layout]);
  Serial.print(" | SwapAB: ");
  Serial.println(swapAB ? "ON" : "OFF");
}

void printButtonEvent(const char* event, uint8_t btnIndex, uint8_t code, bool ext) {
  Serial.print(event);
  Serial.print(" ");
  Serial.print(buttonNames[btnIndex]);
  if (code != NO_KEY) {
    Serial.print(" -> ");
    Serial.print(getKeyName(code));
    Serial.print(" (0x");
    if (code < 0x10) Serial.print('0');
    Serial.print(code, HEX);
    Serial.print(")");
    if (ext) Serial.print(" [EXT]");
    Serial.println();
  } else {
    Serial.println(" (NULL)");
  }
}
#endif

// ============================================================
// SETUP
// ============================================================
void setup() {
  pinMode(NES_LATCH, OUTPUT);
  pinMode(NES_CLOCK, OUTPUT);
  pinMode(NES_DATA, INPUT);
  digitalWrite(NES_LATCH, LOW);
  digitalWrite(NES_CLOCK, HIGH);
  
  #if DEBUG
  Serial.begin(115200);
  while (!Serial);
  Serial.println("--- NES->PS/2 Ready ---");
  #endif
  
  keyboard.keyboard_init();
  loadConfig();
  
  #if DEBUG
  Serial.print("Config: ");
  printConfig();
  Serial.println("Hold SELECT + A/B for SwapAB");
  #endif
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  processQueue();
  uint8_t state = readNES();
  const uint8_t* keys = layouts[layout];
  const bool* ext = extFlags[layout];
  
  // --- SELECT Mode Detection ---
  bool selectPressed = !(state & (1 << 2));
  
  if (selectPressed && !selectMode) {
    selectMode = true;
    releaseAll(keys, ext);
    #if DEBUG
    Serial.println("--- CONFIG MODE ENTERED ---");
    printConfig();
    #endif
  }
  
  if (!selectPressed && selectMode) {
    selectMode = false;
    #if DEBUG
    Serial.println("--- CONFIG MODE EXITED ---");
    #endif
  }
  
  // --- Configuration Mode Logic ---
  if (selectMode) {
    bool aPressed = (!(state & (1 << 0)) && (lastNesState & (1 << 0)));
    bool bPressed = (!(state & (1 << 1)) && (lastNesState & (1 << 1)));
    
    if (aPressed) {
      swapAB = false;
      saveConfig();
      #if DEBUG
      Serial.println("SwapAB: OFF");
      printConfig();
      #endif
      delay(300);
    }
    else if (bPressed) {
      swapAB = true;
      saveConfig();
      #if DEBUG
      Serial.println("SwapAB: ON");
      printConfig();
      #endif
      delay(300);
    }
    else {
      bool layoutChanged = false;
      if (!(state & (1 << 5))) { layout = 0; layoutChanged = true; }      // DOWN
      else if (!(state & (1 << 6))) { layout = 1; layoutChanged = true; } // LEFT
      else if (!(state & (1 << 7))) { layout = 2; layoutChanged = true; } // RIGHT
      else if (!(state & (1 << 4))) { layout = 3; layoutChanged = true; } // UP
      
      if (layoutChanged) {
        // === FIX: Release keys from OLD layout before switching ===
        releaseAll(keys, ext);  // keys/ext still point to OLD layout here
        
        // Fully reset button state to prevent false triggers
        for (int i = 0; i < BUTTON_COUNT; i++) {
          buttonState[i] = false;
        }
        
        saveConfig();
        #if DEBUG
        Serial.print("Layout: ");
        Serial.println(layoutNames[layout]);
        printConfig();
        #endif
        delay(300);
        
        lastNesState = state;
        return;  // === FIX: Skip Normal Operation this iteration ===
      }
    }
    // NO return here! SELECT still sends its key (ESC or F1)
  }
  
  // --- Normal Operation ---
  bool physA = !(state & 0x01);
  bool physB = !(state & 0x02);
  
  bool logicA = swapAB ? physB : physA;
  bool logicB = swapAB ? physA : physB;
  
  for (int i = 0; i < BUTTON_COUNT; i++) {
    bool pressed = false;
    uint8_t bitIndex = nesBitIndex[i];
    
    if (i == 0) {
      pressed = logicA;
    }
    else if (i == 1) {
      pressed = logicB;
    }
    else {
      pressed = !(state & (1 << bitIndex));
    }
    
    if (pressed && !buttonState[i]) {
      sendMake(keys[i], ext[i]);
      buttonState[i] = true;
      #if DEBUG
      printButtonEvent("[PRESS]", i, keys[i], ext[i]);
      #endif
    } else if (!pressed && buttonState[i]) {
      sendBreak(keys[i], ext[i]);
      buttonState[i] = false;
      #if DEBUG
      printButtonEvent("[RELEASE]", i, keys[i], ext[i]);
      #endif
    }
  }
  
  lastNesState = state;
}
