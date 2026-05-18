/**
 * laser_controller_arduino.ino
 * RDC6332G HDI Touchscreen Controller – LVGL 9.5.x
 *
 * Tools → Partition Scheme → Huge APP (3MB No OTA)
 *
 * IMPORTANT BEFORE COMPILING:
 *   Copy lv_conf.h to:
 *   Documents\Arduino\libraries\lvgl\src\lv_conf.h
 */

/* lvgl_arduino.h MUST be the absolute first include */
/* It sets up the standalone LVGL library */
#include "lvgl_arduino.h"

/* lgfx_setup.h contains our custom display driver (does NOT use LovyanGFX library) */
#include "lgfx_setup.h"
#include "config.h"
#include "ruida.h"
#include "ui.h"

/* ── Display singleton ─────────────────────────── */
LGFX gfx;

/* ── LVGL draw buffers ─────────────────────────── */
static const uint32_t BUF_PIXELS = TFT_WIDTH * (TFT_HEIGHT / 10);
static uint8_t buf1[BUF_PIXELS * sizeof(uint16_t)];
static uint8_t buf2[BUF_PIXELS * sizeof(uint16_t)];

/* ── LVGL v9 flush callback ────────────────────── */
static void lvgl_flush_cb(lv_display_t* disp,
                           const lv_area_t* area,
                           uint8_t* px_map)
{
    uint16_t x = area->x1;
    uint16_t y = area->y1;
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    // writeRegion keeps CS asserted for the full address+pixel sequence
    gfx.writeRegion(x, y, w, h, (const uint16_t*)px_map);
    lv_display_flush_ready(disp);
}

/* ── LVGL v9 touch callback ────────────────────── */
static void lvgl_touch_cb(lv_indev_t* /*indev*/, lv_indev_data_t* data)
{
    uint16_t x, y;
    if (gfx.getTouch(&x, &y)) {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = (int32_t)x;
        data->point.y = (int32_t)y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ── LVGL task (core 1) ────────────────────────── */
static void lvgl_task(void* /*pvParams*/)
{
    Serial.println("[LVGL_TASK] Started");
    uint32_t last_ms = 0;
    uint32_t frame_count = 0;
    
    for (;;) {
        try {
            lv_task_handler();
        } catch (...) {
            Serial.println("[LVGL_TASK] ERROR: lv_task_handler crashed!");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        
        frame_count++;
        if (frame_count % 100 == 0) {
            Serial.printf("[LVGL_TASK] Frame count: %u\n", frame_count);
        }
        
        uint32_t now = millis();
        if (now - last_ms >= STATUS_POLL_MS) {
            last_ms = now;
            MachineStatus st = ruida_get_status();
            if (st.valid) {
                ui_update_status(st);
            }
        }
        vTaskDelay(LV_DEF_REFR_PERIOD / portTICK_PERIOD_MS);
    }
}

/* ── setup() ───────────────────────────────────── */
void setup()
{
    Serial.begin(115200);
    Serial.println("\n[BOOT] RDC6332G HDI Controller – LVGL 9.5");
    Serial.printf("[DEBUG] Free heap at start: %u bytes\n", esp_get_free_heap_size());

    /* 1. Display */
    Serial.println("[BOOT] Initializing display...");
    gfx.init();
    Serial.println("[BOOT] Display init OK");
    
    gfx.setRotation(TFT_ROTATION);
    Serial.println("[BOOT] Display rotation OK");
    
    gfx.setBrightness(200);
    Serial.println("[BOOT] Brightness set");
    
    Serial.println("[BOOT] Filling screen black...");
    gfx.fillScreen(0x0000);  // black – TFT_BL is a GPIO pin number, NOT a colour!
    Serial.println("[BOOT] Display OK");
    
    Serial.printf("[DEBUG] Free heap after display: %u bytes\n", esp_get_free_heap_size());

    /* 2. LVGL v9 */
    Serial.println("[BOOT] Initializing LVGL...");
    lv_init();
    Serial.println("[BOOT] LVGL core init OK");

    Serial.println("[BOOT] Creating LVGL display object...");
    lv_display_t* disp = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    if (!disp) {
        Serial.println("[ERROR] Failed to create LVGL display!");
        Serial.printf("[ERROR] Free heap: %u bytes\n", esp_get_free_heap_size());
        return;  // Bail out gracefully
    }
    Serial.println("[BOOT] LVGL display created");

    Serial.println("[BOOT] Setting flush callback...");
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    Serial.println("[BOOT] Flush callback set");

    Serial.printf("[BOOT] Setting display buffers (%u bytes each)...\n", (uint32_t)sizeof(buf1));
    lv_display_set_buffers(disp, buf1, buf2,
                           sizeof(buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("[BOOT] Display buffers configured");

    Serial.println("[BOOT] Creating touch input device...");
    lv_indev_t* indev = lv_indev_create();
    if (!indev) {
        Serial.println("[ERROR] Failed to create touch input device!");
        return;
    }
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
    Serial.println("[BOOT] Touch input OK");

    Serial.println("[BOOT] LVGL 9.5 OK");
    Serial.printf("[DEBUG] Free heap after LVGL: %u bytes\n", esp_get_free_heap_size());

    /* 3. UI */
    Serial.println("[BOOT] Building UI...");
    ui_init();
    Serial.println("[BOOT] UI built");
    Serial.printf("[DEBUG] Free heap after UI: %u bytes\n", esp_get_free_heap_size());

    /* 4. HDI serial */
    Serial.println("[BOOT] Initializing HDI serial...");
    ruida_init();
    Serial.println("[BOOT] HDI serial OK");

    /* 5. Tasks */
    Serial.println("[BOOT] Creating LVGL task...");
    BaseType_t result1 = xTaskCreatePinnedToCore(lvgl_task,  "lvgl", TASK_LVGL_STACK,
                            nullptr, TASK_LVGL_PRIO, nullptr, 1);
    if (result1 != pdPASS) {
        Serial.printf("[ERROR] Failed to create LVGL task! Result: %d\n", result1);
    } else {
        Serial.println("[BOOT] LVGL task created on core 1");
    }

    Serial.println("[BOOT] Creating HDI task...");
    BaseType_t result2 = xTaskCreatePinnedToCore(ruida_task, "hdi",  TASK_HDI_STACK,
                            nullptr, TASK_HDI_PRIO,  nullptr, 0);
    if (result2 != pdPASS) {
        Serial.printf("[ERROR] Failed to create HDI task! Result: %d\n", result2);
    } else {
        Serial.println("[BOOT] HDI task created on core 0");
    }

    Serial.println("[BOOT] Tasks launched");
    Serial.printf("[DEBUG] Free heap after tasks: %u bytes\n", esp_get_free_heap_size());
    Serial.println("[BOOT] ===== STARTUP COMPLETE =====\n");
}

void loop() { vTaskDelay(portMAX_DELAY); }
