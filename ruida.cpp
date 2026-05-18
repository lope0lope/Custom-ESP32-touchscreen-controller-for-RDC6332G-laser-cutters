/**
 * ruida.cpp – RDC6332G HDI serial protocol implementation
 */

#include "ruida.h"
#include "config.h"
#include <Arduino.h>

// ──────────────────────────────────────────────
//  Internal state
// ──────────────────────────────────────────────
static SemaphoreHandle_t  s_status_mutex = nullptr;
static MachineStatus      s_status;

// RX parser state machine
static uint8_t  s_rx_buf[STATUS_PKT_LEN];
static uint8_t  s_rx_idx = 0;
static bool     s_in_packet = false;

// ──────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────
static int32_t parse_int32_be(const uint8_t* p)
{
    return (int32_t)(((uint32_t)p[0] << 24) |
                     ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] <<  8) |
                      (uint32_t)p[3]);
}

static uint16_t parse_uint16_be(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint8_t checksum(const uint8_t* data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += data[i];
    return sum & 0xFF;
}

// ──────────────────────────────────────────────
//  Status packet parser
// ──────────────────────────────────────────────
//  Packet layout (19 bytes total):
//   [0]     0xDA  header
//   [1]     state
//   [2-5]   X position µm  (int32 BE)
//   [6-9]   Y position µm
//   [10-13] Z position µm
//   [14-15] laser power ×10 (uint16 BE)
//   [16-17] progress   ×10 (uint16 BE)
//   [18]    checksum  = sum([1..17]) & 0xFF

static void parse_status_packet(const uint8_t* pkt)
{
    // Verify checksum
    uint8_t expected_cs = checksum(pkt + 1, STATUS_PKT_LEN - 2);
    if (expected_cs != pkt[STATUS_PKT_LEN - 1]) {
        Serial.printf("[HDI] Bad checksum: got 0x%02X expected 0x%02X\n",
                      pkt[STATUS_PKT_LEN - 1], expected_cs);
        return;
    }

    MachineStatus s;
    s.state      = (RuiState)pkt[1];
    s.x_mm       = parse_int32_be(pkt +  2) / 1000.0f;   // µm → mm
    s.y_mm       = parse_int32_be(pkt +  6) / 1000.0f;
    s.z_mm       = parse_int32_be(pkt + 10) / 1000.0f;
    s.power_pct  = parse_uint16_be(pkt + 14) / 10.0f;    // ×10 → %
    s.progress   = parse_uint16_be(pkt + 16) / 10.0f;
    s.valid      = true;

    if (s_status_mutex && xSemaphoreTake(s_status_mutex, 10 / portTICK_PERIOD_MS)) {
        s_status = s;
        xSemaphoreGive(s_status_mutex);
    }
}

// ──────────────────────────────────────────────
//  Public: init
// ──────────────────────────────────────────────
void ruida_init()
{
    s_status_mutex = xSemaphoreCreateMutex();

    Serial2.begin(HDI_BAUD, SERIAL_8N1, HDI_RX_PIN, HDI_TX_PIN);
    Serial2.setRxBufferSize(HDI_RX_BUF);
    Serial.println("[HDI] UART2 initialised");
}

// ──────────────────────────────────────────────
//  Public: background receive task
// ──────────────────────────────────────────────
void ruida_task(void* /*pvParams*/)
{
    for (;;) {
        while (Serial2.available()) {
            uint8_t b = (uint8_t)Serial2.read();

            if (!s_in_packet) {
                // Wait for status header
                if (b == HDI_STATUS_HEADER) {
                    s_in_packet  = true;
                    s_rx_idx     = 0;
                    s_rx_buf[s_rx_idx++] = b;
                }
            } else {
                // Safety: prevent buffer overflow
                if (s_rx_idx < STATUS_PKT_LEN) {
                    s_rx_buf[s_rx_idx++] = b;
                }
                
                if (s_rx_idx >= STATUS_PKT_LEN) {
                    parse_status_packet(s_rx_buf);
                    s_in_packet = false;
                    s_rx_idx    = 0;
                }
            }
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

// ──────────────────────────────────────────────
//  Public: send command
//
//  Builds:  [0xD0] [cmd] [params…] [checksum]
//  Checksum covers bytes from [cmd] through end of params.
// ──────────────────────────────────────────────
void ruida_send_cmd(RuiCmd cmd, const uint8_t* params, size_t param_len)
{
    // Build payload without header
    uint8_t payload[16];
    size_t  plen = 0;

    payload[plen++] = (uint8_t)cmd;
    for (size_t i = 0; i < param_len; i++) {
        payload[plen++] = params[i];
    }
    uint8_t cs = checksum(payload, plen);
    payload[plen++] = cs;

    Serial2.write(HDI_CMD_HEADER);
    Serial2.write(payload, plen);
    Serial2.flush();

#if 0  // set to 1 for raw-hex debug output on Serial
    Serial.printf("[HDI TX] D0");
    for (size_t i = 0; i < plen; i++) Serial.printf(" %02X", payload[i]);
    Serial.println();
#endif
}

// ──────────────────────────────────────────────
//  Convenience wrappers
// ──────────────────────────────────────────────
void ruida_stop()
{
    uint8_t p[] = {0x00};
    ruida_send_cmd(CMD_STOP, p, 1);
}

void ruida_pause()
{
    uint8_t p[] = {0x00};
    ruida_send_cmd(CMD_PAUSE, p, 1);
}

void ruida_start()
{
    uint8_t p[] = {0x00};
    ruida_send_cmd(CMD_START, p, 1);
}

void ruida_frame()
{
    uint8_t p[] = {0x00};
    ruida_send_cmd(CMD_FRAME, p, 1);
}

void ruida_home_all()
{
    uint8_t p[] = {0x00};
    ruida_send_cmd(CMD_HOME_ALL, p, 1);
}

/**
 * Jog axis at specified speed units (mm/s × 1000 from config).
 * The function divides by 1000 and transmits as uint16 big-endian.
 * Example: speed=5000 sends 5 mm/s; speed=0 sends stop-jog.
 */
void ruida_jog(RuiAxis axis, RuiDir dir, uint16_t speed_units)
{
    // Convert from (mm/s × 1000) to actual mm/s for transmission
    uint16_t speed_mm_s = speed_units / 1000U;
    if (speed_mm_s > 0xFFFFU) speed_mm_s = 0xFFFFU; // cap at max uint16
    
    uint8_t p[4] = {
        (uint8_t)axis,
        (uint8_t)dir,
        (uint8_t)(speed_mm_s >> 8),
        (uint8_t)(speed_mm_s & 0xFF)
    };
    ruida_send_cmd(CMD_JOG, p, 4);
}

void ruida_jog_stop(RuiAxis axis)
{
    ruida_jog(axis, DIR_POS, 0);
}

/**
 * Fire laser for duration_ms at power_pct (0-100).
 * Power is encoded ×10 (uint16 BE), duration in ms (uint16 BE).
 */
void ruida_laser_test(uint8_t power_pct, uint16_t duration_ms)
{
    // Clamp power to valid range [0..100]
    if (power_pct > 100) power_pct = 100;
    
    uint16_t pwr = (uint16_t)(power_pct * 10);
    uint8_t p[4] = {
        (uint8_t)(pwr        >> 8),
        (uint8_t)(pwr        & 0xFF),
        (uint8_t)(duration_ms >> 8),
        (uint8_t)(duration_ms & 0xFF)
    };
    ruida_send_cmd(CMD_LASER_TEST, p, 4);
}

MachineStatus ruida_get_status()
{
    MachineStatus copy;
    if (s_status_mutex && xSemaphoreTake(s_status_mutex, 10 / portTICK_PERIOD_MS)) {
        copy = s_status;
        xSemaphoreGive(s_status_mutex);
    }
    return copy;
}
