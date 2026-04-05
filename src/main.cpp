// ============================================================
// SmartVFO - SDR VFO Control Knob, Buttons & CW Keyer
// Converted from CircuitPython to Arduino C for RP2040 (UF2)
// RF.Guru - ON6URE
// ============================================================

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <EEPROM.h>
#include <MIDI.h>
#include <hardware/pwm.h>
#include <hardware/watchdog.h>

#include "config.h"
#include "morse.h"

// ============================================================
// USB MIDI (via TinyUSB for RP2040)
// ============================================================
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// ============================================================
// Global State
// ============================================================
static volatile int      g_wpm           = DEFAULT_WPM;
static volatile bool     g_wpm_adjust    = false;    // WPM adjustment mode active
static volatile bool     g_transmitting  = false;    // CW key is down

// Encoder state
static volatile int      g_enc_position  = 0;
static volatile int      g_enc_last      = 0;

// Buzzer PWM
static uint              g_buzzer_slice;
static uint              g_buzzer_chan;

// ============================================================
// RGB LED (common anode, active low / inverted PWM)
// ============================================================
static void led_set(uint8_t r, uint8_t g, uint8_t b) {
    analogWrite(PIN_LED_R, 255 - r);
    analogWrite(PIN_LED_G, 255 - g);
    analogWrite(PIN_LED_B, 255 - b);
}

static void led_green()  { led_set(0, 255, 0);   }
static void led_red()    { led_set(255, 0, 0);    }
static void led_blue()   { led_set(0, 0, 255);    }
static void led_off()    { led_set(0, 0, 0);      }

// ============================================================
// Buzzer (sidetone)
// ============================================================
static void buzzer_init() {
    gpio_set_function(PIN_BUZZER, GPIO_FUNC_PWM);
    g_buzzer_slice = pwm_gpio_to_slice_num(PIN_BUZZER);
    g_buzzer_chan  = pwm_gpio_to_channel(PIN_BUZZER);

    // Configure for sidetone frequency
    // System clock = 125MHz, we want SIDETONE_FREQ Hz
    uint32_t wrap = (125000000UL / SIDETONE_FREQ) - 1;
    if (wrap > 65535) wrap = 65535;
    pwm_set_wrap(g_buzzer_slice, (uint16_t)wrap);
    pwm_set_chan_level(g_buzzer_slice, g_buzzer_chan, 0); // off
    pwm_set_enabled(g_buzzer_slice, true);
}

static void buzzer_on() {
    uint16_t wrap = pwm_hw->slice[g_buzzer_slice].top;
    pwm_set_chan_level(g_buzzer_slice, g_buzzer_chan, wrap / 2); // 50% duty
}

static void buzzer_off() {
    pwm_set_chan_level(g_buzzer_slice, g_buzzer_chan, 0);
}

// ============================================================
// CW Timing
// ============================================================
static inline uint32_t dit_time_ms() {
    // PARIS = 50 units, 1 minute = 60000 ms
    return 60000UL / ((uint32_t)g_wpm * 50UL);
}

// ============================================================
// CW Key Down / Up (MIDI + Buzzer + LED)
// ============================================================
static void cw_key_down() {
    g_transmitting = true;
    led_red();
    MIDI.sendNoteOn(MIDI_NOTE_CW, 127, MIDI_CHANNEL + 1);
    buzzer_on();
}

static void cw_key_up() {
    g_transmitting = false;
    MIDI.sendNoteOff(MIDI_NOTE_CW, 0, MIDI_CHANNEL + 1);
    buzzer_off();
    if (!g_wpm_adjust) {
        led_green();
    }
}

// ============================================================
// Play a morse pattern for a single character (blocking)
// Used for CQ sidetone during WPM adjustment
// ============================================================
static void play_morse_char(char c) {
    if (c == ' ') {
        delay(4 * dit_time_ms()); // word space (additional to inter-char)
        return;
    }

    uint8_t code = morse_encode(c);
    if (code == 0 || code == 0xFF) return;

    uint8_t len     = morse_len(code);
    uint8_t pattern = morse_pattern(code);

    for (uint8_t i = 0; i < len; i++) {
        if (pattern & (1 << i)) {
            // dah
            led_red();
            buzzer_on();
            delay(3 * dit_time_ms());
        } else {
            // dit
            led_red();
            buzzer_on();
            delay(dit_time_ms());
        }
        buzzer_off();
        led_off();
        delay(dit_time_ms()); // inter-element gap
    }
    delay(2 * dit_time_ms()); // inter-character gap (total 3 dit times)
}

// Non-blocking CQ playback state machine for WPM adjust mode
enum CqState {
    CQ_IDLE,
    CQ_ELEMENT_ON,
    CQ_ELEMENT_GAP,
    CQ_CHAR_GAP,
    CQ_WORD_GAP,
    CQ_MSG_PAUSE
};

static CqState   cq_state     = CQ_IDLE;
static const char *cq_ptr     = nullptr;   // current char in message
static uint8_t   cq_elem_idx  = 0;         // current element within character
static uint8_t   cq_elem_len  = 0;         // total elements in current char
static uint8_t   cq_pattern   = 0;         // bit pattern of current char
static uint32_t  cq_timer     = 0;

static void cq_start() {
    cq_ptr = CQ_MESSAGE;
    cq_state = CQ_IDLE;
}

static void cq_stop() {
    cq_state = CQ_IDLE;
    cq_ptr = nullptr;
    buzzer_off();
}

// Call each loop iteration — drives CQ playback without blocking
static void cq_tick() {
    if (cq_ptr == nullptr) return;
    uint32_t dt = dit_time_ms();
    uint32_t now = millis();

    switch (cq_state) {
    case CQ_IDLE:
        // Start next character
        if (*cq_ptr == '\0') {
            // End of message — pause then restart
            cq_timer = now;
            cq_state = CQ_MSG_PAUSE;
            return;
        }
        if (*cq_ptr == ' ') {
            cq_timer = now;
            cq_state = CQ_WORD_GAP;
            cq_ptr++;
            return;
        }
        {
            uint8_t code = morse_encode(*cq_ptr);
            if (code == 0 || code == 0xFF) {
                cq_ptr++;
                return;
            }
            cq_elem_len = morse_len(code);
            cq_pattern  = morse_pattern(code);
            cq_elem_idx = 0;
            // Start first element
            led_red();
            buzzer_on();
            cq_timer = now;
            cq_state = CQ_ELEMENT_ON;
        }
        break;

    case CQ_ELEMENT_ON: {
        uint32_t dur = (cq_pattern & (1 << cq_elem_idx)) ? 3 * dt : dt;
        if (now - cq_timer >= dur) {
            buzzer_off();
            led_off();
            cq_timer = now;
            cq_state = CQ_ELEMENT_GAP;
        }
        break;
    }

    case CQ_ELEMENT_GAP:
        if (now - cq_timer >= dt) {
            cq_elem_idx++;
            if (cq_elem_idx >= cq_elem_len) {
                // Character done — inter-character gap
                cq_ptr++;
                cq_timer = now;
                cq_state = CQ_CHAR_GAP;
            } else {
                // Next element
                led_red();
                buzzer_on();
                cq_timer = now;
                cq_state = CQ_ELEMENT_ON;
            }
        }
        break;

    case CQ_CHAR_GAP:
        if (now - cq_timer >= 2 * dt) {
            cq_state = CQ_IDLE;
        }
        break;

    case CQ_WORD_GAP:
        if (now - cq_timer >= 4 * dt) {
            cq_state = CQ_IDLE;
        }
        break;

    case CQ_MSG_PAUSE:
        if (now - cq_timer >= 1500) {
            cq_ptr = CQ_MESSAGE; // restart
            cq_state = CQ_IDLE;
        }
        break;
    }
}

// ============================================================
// MIDI Encoder: NoteOn+NoteOff per detent with acceleration
// repeat > 1 when turning fast so the radio tunes quicker
// ============================================================
static void send_encoder_midi(int8_t direction, uint8_t repeat) {
    uint8_t note = (direction > 0) ? MIDI_NOTE_ENC_CW : MIDI_NOTE_ENC_CCW;
    for (uint8_t i = 0; i < repeat; i++) {
        MIDI.sendNoteOn(note, 127, MIDI_CHANNEL + 1);
        MIDI.sendNoteOff(note, 0, MIDI_CHANNEL + 1);
    }
}

// ============================================================
// Iambic Keyer State Machine
// ============================================================
enum IambicState {
    IAMBIC_SPACE,
    IAMBIC_DIT,
    IAMBIC_DIT_WAIT,
    IAMBIC_DAH,
    IAMBIC_DAH_WAIT
};

static IambicState  iam_state    = IAMBIC_SPACE;
static bool         iam_dit_latch = false;
static bool         iam_dah_latch = false;
static bool         iam_in_char  = false;
static bool         iam_in_word  = false;
static uint32_t     iam_timer    = 0;
static char         iam_pattern[8]; // max 7 elements + null
static uint8_t      iam_pat_len  = 0;

static void iam_reset_timer() {
    iam_timer = millis();
}

static uint32_t iam_elapsed() {
    return millis() - iam_timer;
}

static void iam_latch_paddles() {
    if (digitalRead(PIN_DIT) == LOW) iam_dit_latch = true;
    if (digitalRead(PIN_DAH) == LOW) iam_dah_latch = true;
}

static void iam_start_dit() {
    iam_dit_latch = false;
    iam_dah_latch = false;
    iam_in_char = true;
    iam_in_word = true;
    if (iam_pat_len < 7) iam_pattern[iam_pat_len++] = '.';
    cw_key_down();
    iam_reset_timer();
    iam_state = IAMBIC_DIT;
}

static void iam_start_dah() {
    iam_dit_latch = false;
    iam_dah_latch = false;
    iam_in_char = true;
    iam_in_word = true;
    if (iam_pat_len < 7) iam_pattern[iam_pat_len++] = '-';
    cw_key_down();
    iam_reset_timer();
    iam_state = IAMBIC_DAH;
}

// Decode accumulated pattern to character (for serial output)
static char iam_decode_char() {
    // Search through morse_table for matching pattern
    for (int c = 0; c < 96; c++) {
        uint8_t code = morse_table[c];
        if (code == 0 || code == 0xFF) continue;
        uint8_t len = morse_len(code);
        uint8_t pat = morse_pattern(code);
        if (len != iam_pat_len) continue;

        bool match = true;
        for (uint8_t i = 0; i < len; i++) {
            bool is_dah = (pat >> i) & 1;
            if ((is_dah && iam_pattern[i] != '-') ||
                (!is_dah && iam_pattern[i] != '.')) {
                match = false;
                break;
            }
        }
        if (match) {
            return (char)(c + 32);
        }
    }
    return '?';
}

static void iam_cycle() {
    iam_latch_paddles();

    uint32_t dt = dit_time_ms();

    switch (iam_state) {
    case IAMBIC_SPACE:
        if (iam_dit_latch) {
            iam_start_dit();
        } else if (iam_dah_latch) {
            iam_start_dah();
        } else if (iam_in_char && iam_elapsed() > 2 * dt) {
            iam_in_char = false;
            // ── CW → Serial ──────────────────────────────────────
            char decoded = iam_decode_char();
            if (Serial) Serial.print(decoded);
            iam_pat_len = 0;
        } else if (iam_in_word && iam_elapsed() > 6 * dt) {
            iam_in_word = false;
            if (Serial) Serial.print(' ');
        }
        break;

    case IAMBIC_DIT:
        if (iam_elapsed() > dt) {
            cw_key_up();
            iam_dit_latch = false;
            iam_reset_timer();
            iam_state = IAMBIC_DIT_WAIT;
        }
        break;

    case IAMBIC_DIT_WAIT:
        if (iam_elapsed() > dt) {
            if (iam_dah_latch) {
                iam_start_dah();
            } else if (iam_dit_latch) {
                iam_start_dit();
            } else {
                iam_state = IAMBIC_SPACE;
            }
        }
        break;

    case IAMBIC_DAH:
        if (iam_elapsed() > 3 * dt) {
            cw_key_up();
            iam_dah_latch = false;
            iam_reset_timer();
            iam_state = IAMBIC_DAH_WAIT;
        }
        break;

    case IAMBIC_DAH_WAIT:
        if (iam_elapsed() > dt) {
            if (iam_dit_latch) {
                iam_start_dit();
            } else if (iam_dah_latch) {
                iam_start_dah();
            } else {
                iam_state = IAMBIC_SPACE;
            }
        }
        break;
    }
}

// ============================================================
// Rotary Encoder (polling with acceleration)
// ============================================================

// Simple Gray-code encoder reading
static int8_t enc_read_delta() {
    static uint8_t prev_state = 0;
    static int8_t  enc_val    = 0;

    // Read current state
    uint8_t a = digitalRead(PIN_ENC_A);
    uint8_t b = digitalRead(PIN_ENC_B);
    uint8_t cur_state = (a << 1) | b;

    // State transition table for quadrature
    static const int8_t enc_states[] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };

    uint8_t idx = (prev_state << 2) | cur_state;
    enc_val += enc_states[idx & 0x0F];
    prev_state = cur_state;

    if (enc_val >= 4) {
        enc_val -= 4;
        return 1;
    } else if (enc_val <= -4) {
        enc_val += 4;
        return -1;
    }
    return 0;
}

// ============================================================
// Serial input handler (receive text from computer, play as CW)
// ============================================================
// ── Serial → CW ──────────────────────────────────────────────
// Characters typed in the serial terminal are played as CW.
// Enter / CR / LF → word space.  Unknown chars are silently skipped.
static void handle_serial_input() {
    if (!CW_MODE) return;
    if (!Serial) return;

    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\r' || c == '\n') {
            delay(4 * dit_time_ms());
            watchdog_update();
            continue;
        }

        if (c == ' ') {
            delay(4 * dit_time_ms());
        } else {
            uint8_t code = morse_encode(c);   // handles lower/upper case
            if (code != 0 && code != 0xFF) {
                uint8_t len = morse_len(code);
                uint8_t pat = morse_pattern(code);
                for (uint8_t i = 0; i < len; i++) {
                    if (pat & (1 << i)) {
                        cw_key_down();
                        delay(3 * dit_time_ms());
                    } else {
                        cw_key_down();
                        delay(dit_time_ms());
                    }
                    cw_key_up();
                    delay(dit_time_ms());   // inter-element gap
                }
                delay(2 * dit_time_ms()); // inter-character gap (3 total)
            }
        }
        watchdog_update();
    }
}

// ============================================================
// WPM Adjustment Mode
// ============================================================
static uint32_t wpm_blink_timer = 0;
static bool     wpm_blink_state = false;

static void wpm_adjust_enter() {
    g_wpm_adjust = true;
    wpm_blink_timer = millis();
    wpm_blink_state = false;
    led_red();
    cq_start();
}

static void wpm_adjust_exit() {
    g_wpm_adjust = false;
    cq_stop();
    buzzer_off();
    // Persist new WPM to flash
    EEPROM.write(0, (uint8_t)g_wpm);
    EEPROM.commit();
    led_green();
}

// Called in main loop when WPM adjust is active
static void wpm_adjust_tick() {
    if (!g_wpm_adjust) return;

    // Non-blocking CQ playback
    cq_tick();
}

// ============================================================
// Setup
// ============================================================
void setup() {
    // Enable watchdog (2 second timeout)
    watchdog_enable(2000, true);
    watchdog_update();

    // Initialize MIDI before Serial so all USB interfaces are registered
    // before the USB stack enumerates
    usb_midi.setStringDescriptor("SmartVFO");
    MIDI.begin(MIDI_CHANNEL + 1);
    MIDI.turnThruOff();

    // USB Serial
    Serial.begin(115200);

    // RGB LED pins
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    led_blue(); // startup color

    // Encoder pins
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    pinMode(PIN_ENC_BTN, INPUT_PULLUP);

    // Button pins
    pinMode(PIN_BTN_TOP, INPUT_PULLUP);
    pinMode(PIN_BTN_BOT, INPUT_PULLUP);

    // Paddle pins
    pinMode(PIN_DIT, INPUT_PULLUP);
    pinMode(PIN_DAH, INPUT_PULLUP);

    // Load saved WPM from flash-backed EEPROM
    EEPROM.begin(4);
    uint8_t saved_wpm = EEPROM.read(0);
    if (saved_wpm >= MIN_WPM && saved_wpm <= MAX_WPM) {
        g_wpm = saved_wpm;
    }

    // Buzzer
    buzzer_init();

    // Small delay for USB enumeration
    delay(500);
    watchdog_update();

    // Ready
    led_green();
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
    static uint32_t enc_led_timer     = 0;   // when encoder last moved
    static uint32_t btn_enc_debounce  = 0;
    static uint32_t btn_enc_press_ms  = 0;   // when encoder button went down
    static bool     btn_enc_held      = false;
    static uint32_t btn_top_debounce  = 0;
    static uint32_t btn_bot_debounce  = 0;

    watchdog_update();
    MIDI.read(); // keep USB MIDI transport alive

    int8_t delta;

    // ---- WPM Adjustment Mode ----
    if (g_wpm_adjust) {
        // Check encoder for WPM change
        delta = enc_read_delta();
        if (delta != 0) {
            g_wpm += delta;
            if (g_wpm < MIN_WPM) g_wpm = MIN_WPM;
            if (g_wpm > MAX_WPM) g_wpm = MAX_WPM;
            // Restart CQ playback with new speed
            cq_stop();
            cq_start();
        }

        // Exit WPM adjust on a fresh press — ignore the button until it has
        // been fully released first (guards against the long-press entry bleed-through)
        static bool wpm_btn_released = false;
        if (digitalRead(PIN_ENC_BTN) == HIGH) {
            wpm_btn_released = true;
        } else if (wpm_btn_released && millis() - btn_enc_debounce > BUTTON_DEBOUNCE_MS) {
            wpm_btn_released = false;
            btn_enc_debounce = millis();
            wpm_adjust_exit();
            return;
        }

        // Run the non-blocking CQ sidetone
        wpm_adjust_tick();
        return;
    }

    // ---- Rotary Encoder (VFO) with acceleration ----
    delta = enc_read_delta();
    if (delta != 0) {
        uint32_t now = millis();
        static uint32_t enc_last_ms = 0;
        uint32_t elapsed = now - enc_last_ms;
        enc_last_ms = now;
        enc_led_timer = now;

        uint32_t raw = (elapsed > 0) ? (ENC_ACCEL_BASE_MS / elapsed) : ENC_ACCEL_MAX;
        uint8_t repeat = (uint8_t)constrain((int)raw, 1, ENC_ACCEL_MAX);

        if (delta > 0) {
            send_encoder_midi(-1, repeat); // flipped
        } else {
            send_encoder_midi(1,  repeat); // flipped
        }
    }

    // ---- Encoder Button (short = MIDI note 32, long = WPM adjust) ----
    if (digitalRead(PIN_ENC_BTN) == LOW) {
        if (millis() - btn_enc_debounce > BUTTON_DEBOUNCE_MS && !btn_enc_held) {
            // Button just pressed
            btn_enc_press_ms = millis();
            btn_enc_held = true;
        }
        // Trigger long-press immediately once threshold is crossed
        if (btn_enc_held && CW_MODE &&
            millis() - btn_enc_press_ms >= ENC_LONG_PRESS_MS) {
            btn_enc_held = false;
            btn_enc_debounce = millis();
            wpm_adjust_enter();
            return;
        }
    } else if (btn_enc_held) {
        // Button released before long-press threshold — short press
        btn_enc_held = false;
        btn_enc_debounce = millis();
        led_red();
        MIDI.sendNoteOn(MIDI_NOTE_ENC, 127, MIDI_CHANNEL + 1);
        MIDI.sendNoteOff(MIDI_NOTE_ENC, 0, MIDI_CHANNEL + 1);
    }

    // ---- Top Button ----
    if (digitalRead(PIN_BTN_TOP) == LOW && millis() - btn_top_debounce > BUTTON_DEBOUNCE_MS) {
        btn_top_debounce = millis();
        led_blue();
        MIDI.sendNoteOn(MIDI_NOTE_TOP, 127, MIDI_CHANNEL + 1);
        MIDI.sendNoteOff(MIDI_NOTE_TOP, 0, MIDI_CHANNEL + 1);
        delay(BUTTON_DEBOUNCE_MS);
    }

    // ---- Bottom Button (toggle on/off) ----
    static bool btn_bot_toggled = false;
    if (digitalRead(PIN_BTN_BOT) == LOW && millis() - btn_bot_debounce > BUTTON_DEBOUNCE_MS) {
        btn_bot_debounce = millis();
        btn_bot_toggled = !btn_bot_toggled;
        if (btn_bot_toggled) {
            led_red();
            MIDI.sendNoteOn(MIDI_NOTE_BOT, 127, MIDI_CHANNEL + 1);
        } else {
            led_green();
            MIDI.sendNoteOff(MIDI_NOTE_BOT, 0, MIDI_CHANNEL + 1);
        }
        delay(BUTTON_DEBOUNCE_MS);
    }

    // ---- CW Iambic Keyer / Paddle ----
    if (CW_MODE) {
        iam_cycle();
        handle_serial_input();
    } else {
        // Non-CW: paddles as simple note on/off
        static bool dit_state = false;
        static bool dah_state = false;

        if (digitalRead(PIN_DIT) == LOW) {
            if (!dit_state) {
                led_set(0, 255, 0);
                MIDI.sendNoteOn(MIDI_NOTE_DIT, 127, MIDI_CHANNEL + 1);
                dit_state = true;
            }
        } else if (dit_state) {
            led_set(128, 128, 128);
            MIDI.sendNoteOff(MIDI_NOTE_DIT, 0, MIDI_CHANNEL + 1);
            dit_state = false;
        }

        if (digitalRead(PIN_DAH) == LOW) {
            if (!dah_state) {
                led_red();
                MIDI.sendNoteOn(MIDI_NOTE_DAH, 127, MIDI_CHANNEL + 1);
                dah_state = true;
            }
        } else if (dah_state) {
            led_set(128, 128, 128);
            MIDI.sendNoteOff(MIDI_NOTE_DAH, 0, MIDI_CHANNEL + 1);
            dah_state = false;
        }
    }

    // Restore LED if not transmitting and not in special mode
    if (!g_transmitting && !g_wpm_adjust && !btn_bot_toggled) {
        if (digitalRead(PIN_ENC_BTN) == HIGH &&
            digitalRead(PIN_BTN_TOP) == HIGH &&
            digitalRead(PIN_BTN_BOT) == HIGH) {
            if (millis() - enc_led_timer < ENC_LED_HOLD_MS) {
                led_blue(); // hold blue while encoder is moving
            } else {
                led_green();
            }
        }
    }
}
