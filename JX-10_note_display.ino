#include <RoxMux.h>

// ─── Pin Definitions ───────────────────────────────────────────────
#define DATA_PIN 4   // SER  (DS)
#define CLOCK_PIN 3  // SRCLK (SH_CP)
#define LATCH_PIN 2  // RCLK  (ST_CP)
#define LED_PWM -1

// Two daisy-chained 74HC595s = 16 bits, we use 12
// SR 1 (first out) → LEDs 0–7  (lower board notes 0–5 + upper board notes 0–1)
// SR 2 (second out) → LEDs 8–15 (upper board notes 2–5 + unused)
//
// LED mapping: lower board notes 0-5 → bits 0-5
//              upper board notes 0-5 → bits 6-11

#define SR_TOTAL 2
Rox74HC595<SR_TOTAL> sr;

// ─── Protocol Constants ─────────────────────────────────────────────
#define BOARD_LOWER 0xF1
#define BOARD_UPPER 0xF9
#define BOARD_BOTH 0xF4

// ─── State ──────────────────────────────────────────────────────────
uint16_t ledState = 0;  // 12 bits, one per LED

enum ParseState {
  WAIT_BOARD,
  WAIT_CMD,
  WAIT_SKIP2  // discard next 2 bytes (note number + velocity/00)
};

uint8_t skipCount = 0;

ParseState pState = WAIT_BOARD;
uint8_t curBoard = 0;  // active board(s): BOARD_LOWER / BOARD_UPPER / BOARD_BOTH
bool isNoteOn = false;
uint8_t curNote = 0;

// ─── Helpers ────────────────────────────────────────────────────────

// Map (board, noteIndex 0-5) → LED bit 0-11
int ledIndex(uint8_t board, uint8_t noteIdx) {
  if (noteIdx > 5) return -1;
  if (board == BOARD_LOWER) return noteIdx;      // bits 0-5
  if (board == BOARD_UPPER) return noteIdx + 6;  // bits 6-11
  return -1;
}

void setLED(uint8_t board, uint8_t noteIdx, bool on) {
  // Handle BOARD_BOTH by recursing for each
  if (board == BOARD_BOTH) {
    setLED(BOARD_LOWER, noteIdx, on);
    setLED(BOARD_UPPER, noteIdx, on);
    return;
  }
  int idx = ledIndex(board, noteIdx);
  if (idx < 0) return;
  if (on) ledState |= (1 << idx);
  else ledState &= ~(1 << idx);
}

void updateShiftRegisters() {
  // RoxShiftOut pin() sets individual bits then you call update()
  for (int i = 0; i < 16; i++) {
    sr.writePin(i, (ledState >> i) & 1);
  }
  sr.update();
}

// ─── Serial Parser ──────────────────────────────────────────────────
void processSerial() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();

    // Any 0xF1/F4/F9 byte resets board context immediately
    if (b == BOARD_LOWER || b == BOARD_UPPER || b == BOARD_BOTH) {
      curBoard = b;
      pState = WAIT_CMD;
      return;
    }

    switch (pState) {

      case WAIT_BOARD:
        // Ignore until we see a board byte (handled above)
        break;

      case WAIT_CMD:
        {
          if (b >= 0xC0 && b <= 0xC5) {
            setLED(curBoard, b - 0xC0, true);
            updateShiftRegisters();
            pState = WAIT_SKIP2;
          } else if (b >= 0xD0 && b <= 0xD5) {
            setLED(curBoard, b - 0xD0, false);
            updateShiftRegisters();
            pState = WAIT_SKIP2;
          }
          break;
        }

      case WAIT_SKIP2:
        skipCount++;
        if (skipCount >= 2) {
          skipCount = 0;
          pState = WAIT_CMD;  // running status — stay ready for next command
        }
        break;
    }
  }
}

// ─── Setup & Loop ───────────────────────────────────────────────────
void setup() {
  sr.begin(DATA_PIN, LATCH_PIN, CLOCK_PIN, LED_PWM);
  ledState = 0;
  updateShiftRegisters();

  // XIAO RA4M1: Serial1 is the hardware UART (D6=TX, D7=RX)
  Serial1.begin(31250);

  // Optional debug on USB serial
  Serial.begin(115200);
  Serial.println("Polysynth LED ready");
}

void loop() {
  processSerial();
}

// ### Key design decisions explained

// **Protocol parser — state machine with running status**

// The parser handles all the cases you described:

// | Pattern | Handled by |
// |---|---|
// | `0xF1, 0xC0, note, vel` | single note on, lower board |
// | `0xF1, 0xD0, note, 0x00` | single note off |
// | `0xF1, 0xC0, n, v, 0xC1, n, v, ...` | running status, same board byte |
// | `0xF1, 0xC0, n, v, 0xD0, n, 0x00` | mixed on/off, running status |
// | `0xF9, ...` | upper board |
// | `0xF4, ...` | both boards simultaneously |

// After `WAIT_VELOCITY` the parser returns to `WAIT_CMD` (not `WAIT_BOARD`) — this is what enables running status. A new `0xF1/F4/F9` byte will always reset the board context immediately wherever it appears.

// **LED mapping**
// ```
// Bit 0–5  → Lower board, notes 0–5
// Bit 6–11 → Upper board, notes 0–5
