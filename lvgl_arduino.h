#pragma once
/**
 * lvgl_arduino.h
 *
 * Include THIS header as the VERY FIRST include in every .cpp/.h file
 * that uses LVGL. It ensures lv_conf.h is found before lvgl.h loads.
 *
 * This project uses the STANDALONE LVGL library (not LovyanGFX's bundled LVGL).
 *
 * SETUP REQUIRED:
 *   1. Install "LVGL" library by LVGL via Arduino Library Manager
 *   2. Copy lv_conf.h to: Documents\Arduino\libraries\lvgl\src\lv_conf.h
 * 
 * NOTE: Do NOT install LovyanGFX library - it is not needed.
 * The custom LGFX driver only depends on <SPI.h> and <config.h>.
 */

/* Tell LVGL to search for lv_conf.h by name on the include path */
#ifndef LV_CONF_INCLUDE_SIMPLE
  #define LV_CONF_INCLUDE_SIMPLE
#endif

/* Include our config first, then the standalone lvgl library */
#include "lv_conf.h"
#include <lvgl.h>
