/**
 * ui.cpp - LVGL 9.5.x touchscreen UI
 */

/* lvgl_arduino.h MUST be the first include */
#include "lvgl_arduino.h"
#include "ui.h"
#include "ruida.h"
#include "config.h"
#include <Arduino.h>

/* v9 removed lv_coord_t - use int32_t */
typedef int32_t coord_t;

/* ── Colours ────────────────────────────────────── */
#define C_BG         lv_color_hex(0x111318)
#define C_PANEL      lv_color_hex(0x1E2229)
#define C_BTN_MOTION lv_color_hex(0x0077AA)
#define C_BTN_HOME   lv_color_hex(0xBB8800)
#define C_BTN_START  lv_color_hex(0x007700)
#define C_BTN_STOP   lv_color_hex(0xCC0000)
#define C_BTN_PAUSE  lv_color_hex(0xCC6600)
#define C_BTN_LASER  lv_color_hex(0x880088)
#define C_BTN_FRAME  lv_color_hex(0x005588)
#define C_TEXT       lv_color_hex(0xFFFFFF)
#define C_TEXT_DIM   lv_color_hex(0x888888)
#define C_PROG_FILL  lv_color_hex(0x0088FF)

/* ── Widget handles ────────────────────────────── */
static lv_obj_t* s_lbl_state    = nullptr;
static lv_obj_t* s_lbl_x        = nullptr;
static lv_obj_t* s_lbl_y        = nullptr;
static lv_obj_t* s_lbl_z        = nullptr;
static lv_obj_t* s_bar_progress = nullptr;
static lv_obj_t* s_lbl_progress = nullptr;
static lv_obj_t* s_lbl_power    = nullptr;
static lv_obj_t* s_slider_power = nullptr;

static int        s_power_pct  = TEST_FIRE_DEFAULT_PWR;
static lv_timer_t* s_jog_timer = nullptr;
static RuiAxis    s_jog_axis   = AXIS_X;
static RuiDir     s_jog_dir    = DIR_POS;

/* ── Button helper ─────────────────────────────── */
static lv_obj_t* make_btn(lv_obj_t* parent, const char* label,
                           lv_color_t bg, coord_t w, coord_t h)
{
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    /* pressed: slightly darker */
    lv_obj_set_style_bg_color(btn, lv_color_darken(bg, LV_OPA_20), LV_STATE_PRESSED);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);
    return btn;
}

/* ── Jog timer ─────────────────────────────────── */
static void cb_jog_timer(lv_timer_t* /*t*/)
{
    ruida_jog(s_jog_axis, s_jog_dir, JOG_SPEED_FAST);
}

struct JogParams { RuiAxis axis; RuiDir dir; };

static void cb_jog(lv_event_t* e)
{
    JogParams* jp   = (JogParams*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        s_jog_axis = jp->axis;
        s_jog_dir  = jp->dir;
        ruida_jog(jp->axis, jp->dir, JOG_SPEED_FAST);
        if (!s_jog_timer)
            s_jog_timer = lv_timer_create(cb_jog_timer, JOG_REPEAT_MS, nullptr);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ruida_jog_stop(jp->axis);
        if (s_jog_timer) { lv_timer_delete(s_jog_timer); s_jog_timer = nullptr; }
    }
}

/* ── Single-press callbacks ────────────────────── */
static void cb_home     (lv_event_t*) { ruida_home_all(); }
static void cb_start    (lv_event_t*) { ruida_start();    }
static void cb_pause    (lv_event_t*) { ruida_pause();    }
static void cb_stop     (lv_event_t*) { ruida_stop();     }
static void cb_frame    (lv_event_t*) { ruida_frame();    }
static void cb_test_fire(lv_event_t*) { ruida_laser_test((uint8_t)s_power_pct, TEST_FIRE_DURATION_MS); }

static void cb_slider_power(lv_event_t* e)
{
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    s_power_pct = (int)lv_slider_get_value(slider);
    // Ensure power is in valid range [0..100]
    if (s_power_pct < 0) s_power_pct = 0;
    if (s_power_pct > 100) s_power_pct = 100;
    
    if (s_lbl_power) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d %%", s_power_pct);
        lv_label_set_text(s_lbl_power, buf);
    }
}

/* Static jog param instances (must outlive buttons) */
static JogParams jp_x_pos = {AXIS_X, DIR_POS};
static JogParams jp_x_neg = {AXIS_X, DIR_NEG};
static JogParams jp_y_pos = {AXIS_Y, DIR_POS};
static JogParams jp_y_neg = {AXIS_Y, DIR_NEG};
static JogParams jp_z_pos = {AXIS_Z, DIR_POS};
static JogParams jp_z_neg = {AXIS_Z, DIR_NEG};

/* ── ui_init ───────────────────────────────────── */
void ui_init()
{
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Status bar */
    lv_obj_t* sb = lv_obj_create(scr);
    lv_obj_set_size(sb, 320, 30);
    lv_obj_align(sb, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x0A1520), 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_state = lv_label_create(sb);
    lv_label_set_text(s_lbl_state, "IDLE");
    lv_obj_set_style_text_color(s_lbl_state, lv_color_hex(0x00FF88), 0);
    lv_obj_set_style_text_font(s_lbl_state, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_state, LV_ALIGN_LEFT_MID, 6, 0);

    s_lbl_x = lv_label_create(sb);
    lv_label_set_text(s_lbl_x, "X:---");
    lv_obj_set_style_text_color(s_lbl_x, C_TEXT, 0);
    lv_obj_set_style_text_font(s_lbl_x, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_x, LV_ALIGN_LEFT_MID, 58, 0);

    s_lbl_y = lv_label_create(sb);
    lv_label_set_text(s_lbl_y, "Y:---");
    lv_obj_set_style_text_color(s_lbl_y, C_TEXT, 0);
    lv_obj_set_style_text_font(s_lbl_y, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_y, LV_ALIGN_LEFT_MID, 130, 0);

    s_lbl_z = lv_label_create(sb);
    lv_label_set_text(s_lbl_z, "Z:---");
    lv_obj_set_style_text_color(s_lbl_z, C_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_lbl_z, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_z, LV_ALIGN_LEFT_MID, 202, 0);

    /* Motion panel (left) */
    lv_obj_t* mp = lv_obj_create(scr);
    lv_obj_set_size(mp, 162, 182);
    lv_obj_align(mp, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_style_bg_color(mp, C_PANEL, 0);
    lv_obj_set_style_bg_opa(mp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mp, 0, 0);
    lv_obj_set_style_radius(mp, 0, 0);
    lv_obj_set_style_pad_all(mp, 4, 0);
    lv_obj_clear_flag(mp, LV_OBJ_FLAG_SCROLLABLE);

    const coord_t BW=48, BH=52, GAP=3;
    const coord_t R0=4, R1=R0+BH+GAP, R2=R1+BH+GAP;
    const coord_t C0=4, C1=C0+BW+GAP, C2=C1+BW+GAP;

    lv_obj_t* btn_xm = make_btn(mp, "X-", C_BTN_MOTION, BW, BH);
    lv_obj_align(btn_xm, LV_ALIGN_TOP_LEFT, C0, R0);
    lv_obj_add_event_cb(btn_xm, cb_jog, LV_EVENT_PRESSED,    &jp_x_neg);
    lv_obj_add_event_cb(btn_xm, cb_jog, LV_EVENT_RELEASED,   &jp_x_neg);
    lv_obj_add_event_cb(btn_xm, cb_jog, LV_EVENT_PRESS_LOST, &jp_x_neg);

    lv_obj_t* btn_home = make_btn(mp, LV_SYMBOL_HOME, C_BTN_HOME, BW, BH);
    lv_obj_align(btn_home, LV_ALIGN_TOP_LEFT, C1, R0);
    lv_obj_add_event_cb(btn_home, cb_home, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_xp = make_btn(mp, "X+", C_BTN_MOTION, BW, BH);
    lv_obj_align(btn_xp, LV_ALIGN_TOP_LEFT, C2, R0);
    lv_obj_add_event_cb(btn_xp, cb_jog, LV_EVENT_PRESSED,    &jp_x_pos);
    lv_obj_add_event_cb(btn_xp, cb_jog, LV_EVENT_RELEASED,   &jp_x_pos);
    lv_obj_add_event_cb(btn_xp, cb_jog, LV_EVENT_PRESS_LOST, &jp_x_pos);

    lv_obj_t* btn_ym = make_btn(mp, "Y-", C_BTN_MOTION, BW, BH);
    lv_obj_align(btn_ym, LV_ALIGN_TOP_LEFT, C0, R1);
    lv_obj_add_event_cb(btn_ym, cb_jog, LV_EVENT_PRESSED,    &jp_y_neg);
    lv_obj_add_event_cb(btn_ym, cb_jog, LV_EVENT_RELEASED,   &jp_y_neg);
    lv_obj_add_event_cb(btn_ym, cb_jog, LV_EVENT_PRESS_LOST, &jp_y_neg);

    lv_obj_t* btn_yp = make_btn(mp, "Y+", C_BTN_MOTION, BW, BH);
    lv_obj_align(btn_yp, LV_ALIGN_TOP_LEFT, C2, R1);
    lv_obj_add_event_cb(btn_yp, cb_jog, LV_EVENT_PRESSED,    &jp_y_pos);
    lv_obj_add_event_cb(btn_yp, cb_jog, LV_EVENT_RELEASED,   &jp_y_pos);
    lv_obj_add_event_cb(btn_yp, cb_jog, LV_EVENT_PRESS_LOST, &jp_y_pos);

    lv_obj_t* btn_zm = make_btn(mp, "Z-", C_BTN_MOTION, BW, BH);
    lv_obj_align(btn_zm, LV_ALIGN_TOP_LEFT, C0, R2);
    lv_obj_add_event_cb(btn_zm, cb_jog, LV_EVENT_PRESSED,    &jp_z_neg);
    lv_obj_add_event_cb(btn_zm, cb_jog, LV_EVENT_RELEASED,   &jp_z_neg);
    lv_obj_add_event_cb(btn_zm, cb_jog, LV_EVENT_PRESS_LOST, &jp_z_neg);

    lv_obj_t* btn_zp = make_btn(mp, "Z+", C_BTN_MOTION, BW, BH);
    lv_obj_align(btn_zp, LV_ALIGN_TOP_LEFT, C2, R2);
    lv_obj_add_event_cb(btn_zp, cb_jog, LV_EVENT_PRESSED,    &jp_z_pos);
    lv_obj_add_event_cb(btn_zp, cb_jog, LV_EVENT_RELEASED,   &jp_z_pos);
    lv_obj_add_event_cb(btn_zp, cb_jog, LV_EVENT_PRESS_LOST, &jp_z_pos);

    /* Laser / file panel (right) */
    lv_obj_t* lp = lv_obj_create(scr);
    lv_obj_set_size(lp, 156, 182);
    lv_obj_align(lp, LV_ALIGN_TOP_RIGHT, 0, 30);
    lv_obj_set_style_bg_color(lp, C_PANEL, 0);
    lv_obj_set_style_bg_opa(lp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lp, 0, 0);
    lv_obj_set_style_radius(lp, 0, 0);
    lv_obj_set_style_pad_all(lp, 4, 0);
    lv_obj_clear_flag(lp, LV_OBJ_FLAG_SCROLLABLE);

    const coord_t LBW=70, LBH=44, LGAP=4;
    const coord_t LC0=4, LC1=LC0+LBW+LGAP;
    const coord_t LR0=4, LR1=LR0+LBH+LGAP, LR2=LR1+LBH+LGAP;

    lv_obj_t* btn_frame = make_btn(lp, "FRAME", C_BTN_FRAME, LBW, LBH);
    lv_obj_align(btn_frame, LV_ALIGN_TOP_LEFT, LC0, LR0);
    lv_obj_add_event_cb(btn_frame, cb_frame, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_start = make_btn(lp, LV_SYMBOL_PLAY " GO", C_BTN_START, LBW, LBH);
    lv_obj_align(btn_start, LV_ALIGN_TOP_LEFT, LC1, LR0);
    lv_obj_add_event_cb(btn_start, cb_start, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_pause = make_btn(lp, LV_SYMBOL_PAUSE, C_BTN_PAUSE, LBW, LBH);
    lv_obj_align(btn_pause, LV_ALIGN_TOP_LEFT, LC0, LR1);
    lv_obj_add_event_cb(btn_pause, cb_pause, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_stop = make_btn(lp, LV_SYMBOL_STOP, C_BTN_STOP, LBW, LBH);
    lv_obj_align(btn_stop, LV_ALIGN_TOP_LEFT, LC1, LR1);
    lv_obj_add_event_cb(btn_stop, cb_stop, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_test = make_btn(lp, LV_SYMBOL_WARNING " TEST FIRE", C_BTN_LASER, LBW*2+LGAP, LBH);
    lv_obj_align(btn_test, LV_ALIGN_TOP_LEFT, LC0, LR2);
    lv_obj_add_event_cb(btn_test, cb_test_fire, LV_EVENT_CLICKED, nullptr);

    /* Power slider */
    const coord_t SY = LR2 + LBH + LGAP;
    s_lbl_power = lv_label_create(lp);
    char pwr_buf[12];
    snprintf(pwr_buf, sizeof(pwr_buf), "%d %%", s_power_pct);
    lv_label_set_text(s_lbl_power, pwr_buf);
    lv_obj_set_style_text_color(s_lbl_power, C_TEXT, 0);
    lv_obj_set_style_text_font(s_lbl_power, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_power, LV_ALIGN_TOP_RIGHT, -2, SY+5);

    s_slider_power = lv_slider_create(lp);
    lv_obj_set_size(s_slider_power, 100, 16);
    lv_slider_set_range(s_slider_power, 0, 100);
    lv_slider_set_value(s_slider_power, s_power_pct, LV_ANIM_OFF);
    lv_obj_align(s_slider_power, LV_ALIGN_TOP_LEFT, LC0, SY+4);
    lv_obj_add_event_cb(s_slider_power, cb_slider_power, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_style_bg_color(s_slider_power, lv_color_hex(0x334455), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider_power, lv_color_hex(0x0088FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider_power, lv_color_hex(0xFFFFFF), LV_PART_KNOB);

    /* Progress bar (bottom) */
    lv_obj_t* pp = lv_obj_create(scr);
    lv_obj_set_size(pp, 320, 28);
    lv_obj_align(pp, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(pp, lv_color_hex(0x0A0E14), 0);
    lv_obj_set_style_bg_opa(pp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pp, 0, 0);
    lv_obj_set_style_radius(pp, 0, 0);
    lv_obj_set_style_pad_all(pp, 0, 0);
    lv_obj_clear_flag(pp, LV_OBJ_FLAG_SCROLLABLE);

    s_bar_progress = lv_bar_create(pp);
    lv_obj_set_size(s_bar_progress, 200, 12);
    lv_obj_align(s_bar_progress, LV_ALIGN_LEFT_MID, 6, 0);
    lv_bar_set_range(s_bar_progress, 0, 100);
    lv_bar_set_value(s_bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_progress, lv_color_hex(0x223344), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_progress, C_PROG_FILL,            LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_progress, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar_progress, 4, LV_PART_INDICATOR);

    s_lbl_progress = lv_label_create(pp);
    lv_label_set_text(s_lbl_progress, "0 %");
    lv_obj_set_style_text_color(s_lbl_progress, C_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_lbl_progress, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_progress, LV_ALIGN_RIGHT_MID, -6, 0);
}

/* ── ui_update_status ──────────────────────────── */
void ui_update_status(const MachineStatus& st)
{
    if (!st.valid) return;

    const char* ss = "UNKNOWN";
    lv_color_t  sc = lv_color_hex(0xAAAAAA);
    switch (st.state) {
        case MACH_IDLE:    ss="IDLE";    sc=lv_color_hex(0x00CC66); break;
        case MACH_RUNNING: ss="RUNNING"; sc=lv_color_hex(0x00AAFF); break;
        case MACH_PAUSED:  ss="PAUSED";  sc=lv_color_hex(0xFFAA00); break;
        case MACH_STOPPED: ss="STOPPED"; sc=lv_color_hex(0xFF3333); break;
        case MACH_HOMING:  ss="HOMING";  sc=lv_color_hex(0xFFFF00); break;
        default: break;
    }
    lv_label_set_text(s_lbl_state, ss);
    lv_obj_set_style_text_color(s_lbl_state, sc, 0);

    char buf[24];
    snprintf(buf, sizeof(buf), "X:%.1f", st.x_mm); lv_label_set_text(s_lbl_x, buf);
    snprintf(buf, sizeof(buf), "Y:%.1f", st.y_mm); lv_label_set_text(s_lbl_y, buf);
    snprintf(buf, sizeof(buf), "Z:%.1f", st.z_mm); lv_label_set_text(s_lbl_z, buf);

    int32_t p = (int32_t)st.progress;
    if (p < 0) p = 0; if (p > 100) p = 100;
    lv_bar_set_value(s_bar_progress, p, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d %%", p);
    lv_label_set_text(s_lbl_progress, buf);
}
