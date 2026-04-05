#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// SmartVFO Configuration
// ============================================================

// CW Mode: true = iambic keyer mode, false = straight key/PTT mode
#define CW_MODE          true

// CW Words Per Minute (default, adjustable via encoder button 32)
#define DEFAULT_WPM      15

// Min/Max WPM range for adjustment
#define MIN_WPM          5
#define MAX_WPM          50

// Sidetone frequency in Hz
#define SIDETONE_FREQ    880

// ============================================================
// GPIO Pin Assignments (matching SmartVFO hardware)
// ============================================================

// RGB LED pins
#define PIN_LED_R        3
#define PIN_LED_G        4
#define PIN_LED_B        5

// Rotary Encoder
#define PIN_ENC_A        17
#define PIN_ENC_B        16
#define PIN_ENC_BTN      19

// Extra buttons
#define PIN_BTN_TOP      18
#define PIN_BTN_BOT      15

// Paddle / Key inputs
#define PIN_DIT          13
#define PIN_DAH          12

// Buzzer (sidetone)
#define PIN_BUZZER       24

// ============================================================
// MIDI Configuration
// ============================================================

// MIDI channel (0-based, so 0 = channel 1)
#define MIDI_CHANNEL     0

// Encoder rotation as two buttons (matching original CircuitPython behaviour)
#define MIDI_NOTE_ENC_CW   30   // one NoteOn+NoteOff per CW detent
#define MIDI_NOTE_ENC_CCW  31   // one NoteOn+NoteOff per CCW detent

// Encoder button
#define MIDI_NOTE_ENC      32   // encoder push (WPM adjust in CW mode)

// Extra buttons
#define MIDI_NOTE_BOT    33
#define MIDI_NOTE_TOP    34

// Paddle notes (non-CW mode)
#define MIDI_NOTE_DIT    35
#define MIDI_NOTE_DAH    36

// CW key-down note (CW mode TX indicator)
#define MIDI_NOTE_CW     65

// ============================================================
// Timing
// ============================================================

// Encoder acceleration
// repeat = ENC_ACCEL_BASE_MS / elapsed_ms  (clamped to 1..ENC_ACCEL_MAX)
// At slow turning (elapsed >= BASE_MS) → 1 note
// At fast turning (elapsed = BASE_MS/10) → 10 notes, etc.
#define ENC_ACCEL_BASE_MS      100   // reference interval for 1 note
#define ENC_ACCEL_MAX          64   // hard cap on notes per detent

// How long to keep the LED blue after encoder movement (ms)
#define ENC_LED_HOLD_MS        150

// Button debounce (ms)
#define BUTTON_DEBOUNCE_MS     200

// Long-press threshold for encoder button (ms)
#define ENC_LONG_PRESS_MS      800

// WPM blink interval (ms)
#define WPM_BLINK_INTERVAL_MS  300

#endif // CONFIG_H
