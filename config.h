#pragma once
/**
 * config.h – Pin map and tunable constants for CYD ESP32-2432S028
 *            interfaced with an RDC6332G laser controller via HDI port.
 *
 * ── CYD hardware ────────────────────────────────────────────────────
 *  Display : ILI9341  320×240  SPI (VSPI bus)
 *  Touch   : XPT2046          SPI (HSPI bus, separate from display)
 *  LED RGB : GPIO4/16/17      (active-LOW)
 *  LDR     : GPIO34           (analog light sensor, not used here)
 *  CN1     : GPIO35           (spare ADC)
 *  P3      : GPIO21           (backlight PWM)
 *
 * ── HDI wiring (CYD P3 connector → RDC6332G HDI) ───────────────────
 *  GPIO22 (RXD2) ← HDI TX  (controller sends status here)
 *  GPIO27 (TXD2) → HDI RX  (panel sends commands here)
 *  GND   ←→ GND
 *  NOTE: RDC6332G HDI is 3.3 V TTL.  Do NOT connect to RS-232 directly.
 * ─────────────────────────────────────────────────────────────────── */

// ──────────────────────────────────────────────
//  DISPLAY  (VSPI)
// ──────────────────────────────────────────────
#define TFT_SCLK   14
#define TFT_MOSI   13
#define TFT_MISO   12
#define TFT_CS     15
#define TFT_DC      2
#define TFT_RST    -1   // tied to EN
#define TFT_BL     21   // backlight PWM

#define TFT_WIDTH  320
#define TFT_HEIGHT 240
#define TFT_ROTATION 1  // landscape, USB connector on right

// ──────────────────────────────────────────────
//  TOUCH  (HSPI)
// ──────────────────────────────────────────────
#define TOUCH_SCLK  25
#define TOUCH_MOSI  32
#define TOUCH_MISO  39
#define TOUCH_CS    33
#define TOUCH_IRQ   36

// Touch calibration (raw ADC → pixels).
// Run calibration sketch first and paste values here.
#define TOUCH_X_MIN   300
#define TOUCH_X_MAX  3900
#define TOUCH_Y_MIN   200
#define TOUCH_Y_MAX  3900

// ──────────────────────────────────────────────
//  HDI SERIAL  (UART2)
// ──────────────────────────────────────────────
#define HDI_RX_PIN   22
#define HDI_TX_PIN   27
#define HDI_BAUD   115200
#define HDI_UART_NUM  2    // Serial2

// Size of the RX ring-buffer (bytes)
#define HDI_RX_BUF  256

// How often (ms) the firmware polls for a status packet
#define STATUS_POLL_MS  100

// ──────────────────────────────────────────────
//  MOTION
// ──────────────────────────────────────────────
// Jog speeds stored as (mm/s × 1000) for precision.
// ruida_jog() automatically divides by 1000 before transmission.
#define JOG_SPEED_SLOW    5000   //   5 mm/s
#define JOG_SPEED_FAST   50000   //  50 mm/s

// How long (ms) a jog command is held while button is pressed
// A new jog packet is re-sent every JOG_REPEAT_MS
#define JOG_REPEAT_MS  100

// ──────────────────────────────────────────────
//  LASER TEST FIRE
// ──────────────────────────────────────────────
#define TEST_FIRE_DURATION_MS  200   // pulse width for test-fire button
#define TEST_FIRE_DEFAULT_PWR   30   // default power % shown on slider

// ──────────────────────────────────────────────
//  UI COLOURS  (RGB565)
// ──────────────────────────────────────────────
#define COL_BG          0x1082   // near-black
#define COL_PANEL       0x2104   // dark grey
#define COL_BTN_MOTION  0x04FF   // cyan-ish
#define COL_BTN_HOME    0xFFE0   // yellow
#define COL_BTN_START   0x07E0   // green
#define COL_BTN_STOP    0xF800   // red
#define COL_BTN_PAUSE   0xFD20   // orange
#define COL_BTN_LASER   0xF81F   // magenta
#define COL_BTN_FRAME   0x867F   // blue-grey
#define COL_TEXT        0xFFFF   // white
#define COL_TEXT_DIM    0x8410   // grey

// ──────────────────────────────────────────────
//  TASK PRIORITIES / STACK SIZES
// ──────────────────────────────────────────────
// Was 4096 (16 KB) — not enough for LVGL 9's draw stack
#define TASK_LVGL_STACK   12288   // 48 KB — safe headroom for LVGL 9
#define TASK_HDI_STACK     4096   // HDI is simple, 16 KB is fine
#define TASK_LVGL_PRIO       2
#define TASK_HDI_PRIO        3   // higher than UI so comms stays snappy
