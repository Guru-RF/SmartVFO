#ifndef MORSE_H
#define MORSE_H

#include <stdint.h>

// Morse encoding table
// Each character stored as a uint8_t bitmask:
//   bits 0-4: morse elements (0=dit, 1=dah), LSB first
//   bits 5-7: length (1-7 elements)
// Special: 0x00 = unknown/unmapped

// Helper macros
#define MORSE_ENCODE(len, pattern) ((uint8_t)(((len) << 5) | ((pattern) & 0x1F)))

// Decode a morse_code byte: returns length in elements
static inline uint8_t morse_len(uint8_t code) { return (code >> 5) & 0x07; }
// Returns pattern bits (LSB = first element, 0=dit, 1=dah)
static inline uint8_t morse_pattern(uint8_t code) { return code & 0x1F; }

// Lookup table indexed by ASCII value (32-95 covers space through '_')
// We map lowercase same as uppercase
static const uint8_t morse_table[96] = {
    // 32: space - special handling (word gap)
    0xFF,
    // 33-47: punctuation !"#$%&'()*+,-./
    0, 0, 0, 0, 0,
    MORSE_ENCODE(5, 0b01000),  // & = .-...  (dit dah dit dit dit)
    0,
    MORSE_ENCODE(5, 0b10110),  // ( = -.--.  (dah dit dah dah dit)
    0,
    MORSE_ENCODE(5, 0b01010),  // * = ...-. (understood /SN)
    MORSE_ENCODE(5, 0b01010),  // + = .-.-. (AR)
    MORSE_ENCODE(6, 0b110011), // , = --..--
    MORSE_ENCODE(6, 0b100001), // - = -....-
    MORSE_ENCODE(6, 0b101010), // . = .-.-.-
    MORSE_ENCODE(5, 0b10010),  // / = -..-.

    // 48-57: digits 0-9
    MORSE_ENCODE(5, 0b11111),  // 0 = -----
    MORSE_ENCODE(5, 0b11110),  // 1 = .----
    MORSE_ENCODE(5, 0b11100),  // 2 = ..---
    MORSE_ENCODE(5, 0b11000),  // 3 = ...--
    MORSE_ENCODE(5, 0b10000),  // 4 = ....-
    MORSE_ENCODE(5, 0b00000),  // 5 = .....
    MORSE_ENCODE(5, 0b00001),  // 6 = -....
    MORSE_ENCODE(5, 0b00011),  // 7 = --...
    MORSE_ENCODE(5, 0b00111),  // 8 = ---..
    MORSE_ENCODE(5, 0b01111),  // 9 = ----.

    // 58-64: :;<=>?@
    0, 0, 0,
    MORSE_ENCODE(5, 0b10001),  // = = -...- (/BT)
    0,
    MORSE_ENCODE(6, 0b001100), // ? = ..--..
    MORSE_ENCODE(6, 0b010110), // @ = .--.-.

    // 65-90: A-Z
    MORSE_ENCODE(2, 0b10),     // A = .-
    MORSE_ENCODE(4, 0b0001),   // B = -...
    MORSE_ENCODE(4, 0b0101),   // C = -.-.
    MORSE_ENCODE(3, 0b001),    // D = -..
    MORSE_ENCODE(1, 0b0),      // E = .
    MORSE_ENCODE(4, 0b0100),   // F = ..-.
    MORSE_ENCODE(3, 0b011),    // G = --.
    MORSE_ENCODE(4, 0b0000),   // H = ....
    MORSE_ENCODE(2, 0b00),     // I = ..
    MORSE_ENCODE(4, 0b1110),   // J = .---
    MORSE_ENCODE(3, 0b101),    // K = -.-
    MORSE_ENCODE(4, 0b0010),   // L = .-..
    MORSE_ENCODE(2, 0b11),     // M = --
    MORSE_ENCODE(2, 0b01),     // N = -.
    MORSE_ENCODE(3, 0b111),    // O = ---
    MORSE_ENCODE(4, 0b0110),   // P = .--.
    MORSE_ENCODE(4, 0b1011),   // Q = --.-
    MORSE_ENCODE(3, 0b010),    // R = .-.
    MORSE_ENCODE(3, 0b000),    // S = ...
    MORSE_ENCODE(1, 0b1),      // T = -
    MORSE_ENCODE(3, 0b100),    // U = ..-
    MORSE_ENCODE(4, 0b1000),   // V = ...-
    MORSE_ENCODE(3, 0b110),    // W = .--
    MORSE_ENCODE(4, 0b1001),   // X = -..-
    MORSE_ENCODE(4, 0b1101),   // Y = -.--
    MORSE_ENCODE(4, 0b0011),   // Z = --..

    // 91-95: [\]^_
    0, 0, 0, 0, 0
};

// Get morse code for a character (returns 0 if unknown)
static inline uint8_t morse_encode(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;  // to uppercase
    if (c < 32 || c > 95) return 0;
    return morse_table[c - 32];
}

// CQ message for WPM calibration sidetone
#define CQ_MESSAGE "CQ CQ CQ"

#endif // MORSE_H
