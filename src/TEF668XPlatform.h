#pragma once
/**
 * @file TEF668XPlatform.h
 * @brief Tiny portability layer: constant tables live in flash on every board.
 *
 * AVR and ESP8266 keep `const` data in RAM unless it is marked PROGMEM and read
 * with pgm_read_*(). ESP32 and ARM keep `const` data in flash already, so the
 * macros fall back to plain reads there. Only the library tables use this file.
 */
#include <stdint.h>

#if defined(__AVR__) || defined(ESP8266)
#if defined(__AVR__)
#include <avr/pgmspace.h>
#else
#include <pgmspace.h>
#endif
#define TEF668X_PROGMEM PROGMEM
#define TEF668X_READ_U8(p) pgm_read_byte(p)
#define TEF668X_READ_U16(p) pgm_read_word(p)
#else
#define TEF668X_PROGMEM
#define TEF668X_READ_U8(p) (*(const uint8_t*)(p))
#define TEF668X_READ_U16(p) (*(const uint16_t*)(p))
#endif
