/**
 * lv_conf.h  –  LVGL 9.5.0 configuration
 *
 * IMPORTANT: Copy this file to TWO places:
 *   1. Your sketch folder  (already here)
 *   2. C:\Users\<you>\Documents\Arduino\libraries\lvgl\src\lv_conf.h
 */

#if 1

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ── Colour ──────────────────────────────────────── */
#define LV_COLOR_DEPTH 16

/* ── Memory ──────────────────────────────────────── */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (48U * 1024U)

/* ── Tick ────────────────────────────────────────── */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE  <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* ── Display / input ─────────────────────────────── */
#define LV_DEF_REFR_PERIOD       20
#define LV_DPI_DEF              130
#define LV_INDEV_DEF_READ_PERIOD 30

/* ── Rendering ───────────────────────────────────── */
#define LV_USE_DRAW_SW          1
#define LV_USE_DRAW_SW_COMPLEX  1
#define LV_DRAW_BUF_ALIGN       4

/* ── Fonts ───────────────────────────────────────── */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_DEFAULT        &lv_font_montserrat_14

/* ── Base widgets we USE ─────────────────────────── */
#define LV_USE_ARC       1
#define LV_USE_BAR       1
#define LV_USE_BUTTON    1
#define LV_USE_LABEL     1
#define LV_USE_LINE      1
#define LV_USE_SLIDER    1
#define LV_USE_IMAGE     1   /* required by LV_USE_ANIMIMG even when animimg=0 */

/* ── Widgets we do NOT use ───────────────────────── */
#define LV_USE_ANIMIMG    0   /* needs IMAGE – kept off */
#define LV_USE_CALENDAR   0   /* needs DROPDOWN */
#define LV_USE_SPINBOX    0   /* needs TEXTAREA */
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_KEYBOARD   0   /* needs TEXTAREA – not used in this UI */
#define LV_USE_ROLLER     0
#define LV_USE_SWITCH     0
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0
#define LV_USE_CHART      0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0
#define LV_USE_LED        0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_SPAN       0

/* ── Themes ──────────────────────────────────────── */
#define LV_USE_THEME_DEFAULT 0   /* has keyboard dependency; not needed for this UI */
#define LV_USE_THEME_SIMPLE  1   /* minimal, no extra dependencies */
#define LV_USE_THEME_MONO    0

/* ── Layouts ─────────────────────────────────────── */
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/* ── Animations ──────────────────────────────────── */
#define LV_USE_ANIM 1

/* ── Misc ────────────────────────────────────────── */
#define LV_USE_LOG               0
#define LV_USE_ASSERT_NULL       1
#define LV_USE_ASSERT_MALLOC     1
#define LV_USE_ASSERT_STYLE      0
#define LV_USE_PERF_MONITOR      0
#define LV_USE_MEM_MONITOR       0
#define LV_USE_SYSMON            0
#define LV_USE_OBSERVER          0
#define LV_USE_FRAGMENT          0

#endif /* LV_CONF_H */
#endif /* Enable */
