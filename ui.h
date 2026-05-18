#pragma once
/**
 * ui.h - LVGL UI declarations
 * lvgl_arduino.h MUST be included first - done here so all includers are covered.
 */

#include "lvgl_arduino.h"   /* MUST be first LVGL include */
#include "ruida.h"

void ui_init();
void ui_update_status(const MachineStatus& st);
