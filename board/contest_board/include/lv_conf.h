/**
 * @file lv_conf.h
 * Minimal LVGL configuration for openvela NuttX.
 * Most settings are controlled by Kconfig (CONFIG_LV_*).
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <nuttx/config.h>

/* Color depth */
#ifdef CONFIG_LV_COLOR_DEPTH_32
#define LV_COLOR_DEPTH 32
#elif defined(CONFIG_LV_COLOR_DEPTH_16)
#define LV_COLOR_DEPTH 16
#else
#define LV_COLOR_DEPTH 16
#endif

/* Hor/Ver resolution */
#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 320

/* DPI */
#define LV_DPI_DEF 130

/* Memory settings */
#define LV_MEM_CUSTOM 1

/* Tick rate */
#define LV_DISP_DEF_REFR_PERIOD 16

/* Font settings */
#ifdef CONFIG_LV_FONT_MONTSERRAT_16
#define LV_FONT_MONTSERRAT_16 1
#endif
#ifdef CONFIG_LV_FONT_MONTSERRAT_48
#define LV_FONT_MONTSERRAT_48 1
#endif

/* Feature toggles */
#define LV_USE_LOG 1
#define LV_USE_ANIMIMG 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_IMG 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 1

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

/* Draw settings */
#define LV_DRAW_BUF_ALIGN 64

#endif /* LV_CONF_H */
