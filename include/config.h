#pragma once
// Physical wiring: rows from bottom to top, first LED at bottom left.
constexpr uint8_t LED_PIN = 2; // GPIO2 / D4 on NodeMCU and D1 mini
constexpr uint16_t MATRIX_WIDTH = 16;
constexpr uint16_t MATRIX_HEIGHT = 16;
constexpr bool SERPENTINE = true;
constexpr bool VERTICAL_WIRING = false;
constexpr bool FLIP_X = false;
constexpr bool FLIP_Y = false;
constexpr uint16_t MAX_MILLIAMPS = 2000; // Set to match your 5V supply.
constexpr uint8_t FIRE_SCALE = 70;
constexpr uint16_t FIRE_FRAME_MS = 35;
constexpr uint16_t LED_COUNT = MATRIX_WIDTH * MATRIX_HEIGHT;
static_assert(MATRIX_WIDTH > 0 && MATRIX_WIDTH <= 100, "Width must be 1..100");
static_assert(MATRIX_HEIGHT >= 4 && MATRIX_HEIGHT <= 100, "Height must be 4..100");
static_assert(LED_COUNT <= 1024, "ESP8266: at most 1024 LEDs");
