#pragma once
/**
 * ruida.h – RDC6332G HDI serial protocol
 *
 * ── Protocol overview ────────────────────────────────────────────────
 * The HDI (Human Device Interface) port on RuiDa RDC controllers
 * carries a binary serial protocol used by the OEM touchscreen panel.
 *
 * BAUD   : 115200, 8N1, TTL 3.3 V
 * ENDIAN : big-endian for all multi-byte fields
 *
 * ── Packet formats ───────────────────────────────────────────────────
 *
 * Command (ESP32 → controller)
 *   [0] 0xD0  header
 *   [1] CMD   command byte
 *   [n] …     optional parameter bytes (command-specific)
 *   [last] checksum = (sum of bytes [1..last-1]) & 0xFF
 *
 * Status (controller → ESP32)
 *   Sent by the controller ~10 Hz without being asked.
 *   [0]  0xDA  header
 *   [1]  state
 *   [2-5]  X position  (int32, µm)
 *   [6-9]  Y position  (int32, µm)
 *   [10-13] Z position (int32, µm)
 *   [14-15] laser power ×10  (uint16, e.g. 500 = 50.0 %)
 *   [16-17] file progress ×10 (uint16, e.g. 750 = 75.0 %)
 *   [18]   checksum = (sum of bytes [1..17]) & 0xFF
 *
 * ── DISCLAIMER ───────────────────────────────────────────────────────
 * The HDI protocol is proprietary and undocumented by RuiDa.
 * The values below are derived from community reverse-engineering
 * (primarily meerk40t, rd-usb-serial, and LightBurn forum threads).
 * Your controller firmware version may use slightly different bytes.
 * Enable DEBUG_HDI in config.h and capture raw bytes to verify.
 * ─────────────────────────────────────────────────────────────────── */

#include <Arduino.h>
#include <stdint.h>

// ──────────────────────────────────────────────
//  PACKET CONSTANTS
// ──────────────────────────────────────────────
constexpr uint8_t HDI_CMD_HEADER    = 0xD0;
constexpr uint8_t HDI_STATUS_HEADER = 0xDA;
constexpr uint8_t STATUS_PKT_LEN    = 19;  // bytes including header

// ──────────────────────────────────────────────
//  COMMAND BYTES  (byte [1] of command packet)
// ──────────────────────────────────────────────
enum RuiCmd : uint8_t {
    CMD_STOP        = 0x08,   // []                  – E-stop
    CMD_PAUSE       = 0x09,   // [0x00]              – pause / resume toggle
    CMD_START       = 0x0A,   // [0x00]              – start loaded file
    CMD_FRAME       = 0x0B,   // [0x00]              – trace bounding box
    CMD_HOME_ALL    = 0x07,   // [0x00]              – home all axes
    CMD_JOG         = 0x06,   // [axis][dir][sH][sL] – jog one axis
    CMD_LASER_TEST  = 0x0C,   // [pwrH][pwrL][durH][durL] – test pulse
};

// ──────────────────────────────────────────────
//  JOG PARAMETERS
// ──────────────────────────────────────────────
enum RuiAxis : uint8_t {
    AXIS_X = 0x00,
    AXIS_Y = 0x01,
    AXIS_Z = 0x02,
};

enum RuiDir : uint8_t {
    DIR_POS = 0x00,   // positive direction
    DIR_NEG = 0x01,   // negative direction
};

// ──────────────────────────────────────────────
//  MACHINE STATE CODES  (byte [1] of status packet)
// ──────────────────────────────────────────────
enum RuiState : uint8_t {
    MACH_IDLE    = 0x00,
    MACH_RUNNING = 0x01,
    MACH_PAUSED  = 0x02,
    MACH_STOPPED = 0x03,
    MACH_HOMING  = 0x04,
    MACH_UNKNOWN = 0xFF,
};

// ──────────────────────────────────────────────
//  STATUS DATA  (parsed from status packet)
// ──────────────────────────────────────────────
struct MachineStatus {
    RuiState state      = MACH_UNKNOWN;
    float    x_mm       = 0.0f;   // mm
    float    y_mm       = 0.0f;
    float    z_mm       = 0.0f;
    float    power_pct  = 0.0f;   // 0-100 %
    float    progress   = 0.0f;   // 0-100 %
    bool     valid      = false;  // true once first packet received
};

// ──────────────────────────────────────────────
//  PUBLIC API
// ──────────────────────────────────────────────

/** Initialise UART2 for HDI communication. */
void ruida_init();

/** Call from a background task – drains RX FIFO, parses packets. */
void ruida_task(void* pvParams);

/** Send pre-built command bytes (adds header & checksum internally). */
void ruida_send_cmd(RuiCmd cmd, const uint8_t* params, size_t param_len);

// ── Convenience wrappers ────────────────────────────────────────────
void ruida_stop();
void ruida_pause();
void ruida_start();
void ruida_frame();
void ruida_home_all();
void ruida_jog(RuiAxis axis, RuiDir dir, uint16_t speed_mm_s);
void ruida_jog_stop(RuiAxis axis);
void ruida_laser_test(uint8_t power_pct, uint16_t duration_ms);

/** Read the latest parsed status (thread-safe copy). */
MachineStatus ruida_get_status();
