#pragma once
/**
 * lgfx_setup.h – Minimal ILI9341 display driver (no LovyanGFX class)
 *
 * Uses raw SPI to drive the display, avoiding LovyanGFX's bundled LVGL
 * which conflicts with the standalone LVGL library.
 *
 *  Display : ILI9341  (VSPI)
 *  Touch   : XPT2046  (HSPI – separate bus)
 */

#include <SPI.h>
#include "config.h"

/* ── ILI9341 SPI Display Driver ─────────────────── */
class LGFX {
private:
    SPIClass* _vspi;  // VSPI for display
    SPIClass* _hspi;  // HSPI for touch
    
public:
    LGFX() : _vspi(nullptr), _hspi(nullptr) {}
    
    void init() {
        Serial.println("[LGFX] Initializing display SPI...");
        
        // ── VSPI (Display) ──────────────────────────
        _vspi = new SPIClass(VSPI);
        _vspi->begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
        Serial.println("[LGFX] VSPI initialized");
        
        // Configure GPIO
        pinMode(TFT_DC, OUTPUT);
        digitalWrite(TFT_DC, HIGH);
        pinMode(TFT_CS, OUTPUT);
        digitalWrite(TFT_CS, HIGH);
        if (TFT_RST >= 0) {
            pinMode(TFT_RST, OUTPUT);
            digitalWrite(TFT_RST, HIGH);
        }
        pinMode(TFT_BL, OUTPUT);
        Serial.println("[LGFX] GPIO configured");
        
        // ── Initialize ILI9341 ─────────────────────
        _spi_command(0x01);  // Reset
        delay(5);
        _spi_command(0x28);  // Display OFF
        _spi_command(0x3A);  // Pixel Format: 16-bit
        _spi_data8(0x55);
        _spi_command(0x36);  // Memory Access Control
        _spi_data8(0x08);    // BGR, normal order
        _spi_command(0x13);  // Normal Display Mode
        _spi_command(0x11);  // Exit Sleep
        delay(120);
        _spi_command(0x29);  // Display ON
        Serial.println("[LGFX] ILI9341 initialized");
        
        fillScreen(0x0000);  // Clear to black
        Serial.println("[LGFX] Display ready");
        
        // ── HSPI (Touch) ────────────────────────────
        _hspi = new SPIClass(HSPI);
        _hspi->begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
        pinMode(TOUCH_CS, OUTPUT);
        digitalWrite(TOUCH_CS, HIGH);
        if (TOUCH_IRQ >= 0) {
            pinMode(TOUCH_IRQ, INPUT);
        }
        Serial.println("[LGFX] HSPI (touch) initialized");
    }
    
    void setRotation(uint8_t r) {
        // Map display rotation
        // 1 = landscape (USB on right)
        uint8_t madctl;
        switch (r) {
            case 0: madctl = 0x08; break;      // portrait
            case 1: madctl = 0x28; break;      // landscape – MX=0 fixes horizontal mirror
            case 2: madctl = 0xC8; break;      // rotate 180
            case 3: madctl = 0xA8; break;      // rotate 270
            default: madctl = 0x08; break;
        }
        _spi_command(0x36);
        _spi_data8(madctl);
    }
    
    void setBrightness(uint8_t level) {
        // PWM on TFT_BL (GPIO21)
        // Simple on/off for now
        if (level > 128) {
            digitalWrite(TFT_BL, HIGH);
        } else {
            digitalWrite(TFT_BL, LOW);
        }
    }
    
    void fillScreen(uint16_t color) {
        _write_region(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
        uint32_t pixels = (uint32_t)TFT_WIDTH * TFT_HEIGHT;
        uint8_t h = color >> 8;
        uint8_t l = color & 0xFF;
        
        _vspi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
        digitalWrite(TFT_CS, LOW);
        digitalWrite(TFT_DC, HIGH);  // Data
        
        for (uint32_t i = 0; i < pixels; i++) {
            _vspi->write(h);
            _vspi->write(l);
        }
        
        digitalWrite(TFT_CS, HIGH);
        _vspi->endTransaction();
    }
    
    /**
     * writeRegion – atomic flush for the LVGL flush callback.
     * Keeps CS asserted continuously from the address commands through the
     * last pixel byte.  Splitting this across startWrite/setAddrWindow/
     * writePixels leaves CS HIGH before the pixels are sent, so the display
     * silently discards them.
     */
    void writeRegion(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const uint16_t* pixels)
    {
        _vspi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
        digitalWrite(TFT_CS, LOW);

        // Column address set (0x2A)
        digitalWrite(TFT_DC, LOW);  _vspi->write(0x2A);  digitalWrite(TFT_DC, HIGH);
        uint16_t x2 = x + w - 1;
        _vspi->write16(x);  _vspi->write16(x2);

        // Page address set (0x2B)
        digitalWrite(TFT_DC, LOW);  _vspi->write(0x2B);  digitalWrite(TFT_DC, HIGH);
        uint16_t y2 = y + h - 1;
        _vspi->write16(y);  _vspi->write16(y2);

        // Memory write command then pixel data – CS stays LOW throughout
        digitalWrite(TFT_DC, LOW);  _vspi->write(0x2C);  digitalWrite(TFT_DC, HIGH);
        uint32_t count = (uint32_t)w * h;
        for (uint32_t i = 0; i < count; i++) {
            _vspi->write16(pixels[i]);
        }

        digitalWrite(TFT_CS, HIGH);
        _vspi->endTransaction();
    }

    void startWrite() {
        _vspi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
        digitalWrite(TFT_CS, LOW);
    }
    
    void endWrite() {
        digitalWrite(TFT_CS, HIGH);
        _vspi->endTransaction();
    }
    
    void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
        _write_region(x, y, x + w - 1, y + h - 1);
    }
    
    void writePixels(uint16_t* pixels, uint32_t count) {
        digitalWrite(TFT_DC, HIGH);  // Data
        for (uint32_t i = 0; i < count; i++) {
            _vspi->write(pixels[i] >> 8);
            _vspi->write(pixels[i] & 0xFF);
        }
    }
    
    bool getTouch(uint16_t* x, uint16_t* y) {
        // IRQ line is active-LOW when the panel is touched.
        // GPIO 34-39 on the ESP32 have no internal pull-up, so only trust
        // a definitive LOW reading.
        if (TOUCH_IRQ >= 0 && digitalRead(TOUCH_IRQ) != LOW) {
            return false;
        }

        // XPT2046 SPI: 2 MHz, MODE0, MSB first.
        // Each measurement: send 1 command byte → clock out 16 bits → take
        // the top 12 bits of the response (leading zero + 12 data + 3 trailing).
        _hspi->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
        digitalWrite(TOUCH_CS, LOW);

        // Take 4 averaged samples of each axis to reduce noise.
        uint32_t sum_rx = 0, sum_ry = 0;
        const uint8_t SAMPLES = 4;
        for (uint8_t i = 0; i < SAMPLES; i++) {
            // 0xD0 = Start|A2|A0 → channel X- (differential)
            _hspi->transfer(0xD0);
            uint16_t hi = _hspi->transfer(0x00);
            uint16_t lo = _hspi->transfer(0x00);
            sum_rx += ((hi << 5) | (lo >> 3)) & 0x0FFF;

            // 0x90 = Start|A0    → channel Y- (differential)
            _hspi->transfer(0x90);
            hi = _hspi->transfer(0x00);
            lo = _hspi->transfer(0x00);
            sum_ry += ((hi << 5) | (lo >> 3)) & 0x0FFF;
        }

        // Power-down: send 0x00 to release PENIRQ
        _hspi->transfer(0x00);
        digitalWrite(TOUCH_CS, HIGH);
        _hspi->endTransaction();

        uint16_t raw_x = sum_rx / SAMPLES;   // XPT2046 X channel
        uint16_t raw_y = sum_ry / SAMPLES;   // XPT2046 Y channel

        // Reject out-of-range reads (noise, finger lift artefacts)
        if (raw_x < TOUCH_X_MIN || raw_x > TOUCH_X_MAX) return false;
        if (raw_y < TOUCH_Y_MIN || raw_y > TOUCH_Y_MAX) return false;

        // Map raw ADC → screen pixels.
        // In landscape (rotation 1) the XPT2046 axes are swapped relative
        // to the display axes on the CYD board:
        //   XPT X channel → screen Y   (inverted: X_MAX maps to screen top)
        //   XPT Y channel → screen X
        *x = (uint16_t)map((long)raw_y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, TFT_WIDTH  - 1);
        *y = (uint16_t)map((long)raw_x, TOUCH_X_MAX, TOUCH_X_MIN, 0, TFT_HEIGHT - 1);

        return true;
    }

private:
    void _spi_command(uint8_t cmd) {
        _vspi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
        digitalWrite(TFT_CS, LOW);
        digitalWrite(TFT_DC, LOW);  // Command
        _vspi->write(cmd);
        digitalWrite(TFT_CS, HIGH);
        _vspi->endTransaction();
    }
    
    void _spi_data8(uint8_t data) {
        _vspi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
        digitalWrite(TFT_CS, LOW);
        digitalWrite(TFT_DC, HIGH);  // Data
        _vspi->write(data);
        digitalWrite(TFT_CS, HIGH);
        _vspi->endTransaction();
    }
    
    void _write_region(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
        // Column address
        _spi_command(0x2A);
        _spi_data8(x1 >> 8);
        _spi_data8(x1 & 0xFF);
        _spi_data8(x2 >> 8);
        _spi_data8(x2 & 0xFF);
        
        // Page address
        _spi_command(0x2B);
        _spi_data8(y1 >> 8);
        _spi_data8(y1 & 0xFF);
        _spi_data8(y2 >> 8);
        _spi_data8(y2 & 0xFF);
        
        _spi_command(0x2C);  // Memory write
    }
};

extern LGFX gfx;
