#include <nuttx/config.h>
#include <stdio.h>
#include <time.h>
#include <syslog.h>
#include "lvgl/lvgl.h"
#include "scene_ui.h"
#include "device_state.h"

static lv_obj_t *g_lbl_scene;
static lv_obj_t *g_lbl_conf;
static lv_obj_t *g_lbl_time;
static lv_obj_t *g_lbl_anomaly;
static lv_obj_t *g_dev_cont;

static void update_devices_panel(void)
{
  lv_obj_clean(g_dev_cont);
  int cnt; device_entry_t *devs = device_state_get_devices(&cnt);
  for (int i = 0; i < cnt; i++)
    {
      char buf[64];
      snprintf(buf, sizeof(buf), "%s: %s %d%%",
               devs[i].name,
               devs[i].on ? "ON" : "off",
               devs[i].brightness);
      lv_obj_t *lbl = lv_label_create(g_dev_cont);
      lv_label_set_text(lbl, buf);
      lv_obj_set_style_text_color(lbl,
        devs[i].on ? lv_color_hex(0x00C853) : lv_color_hex(0x9E9E9E), 0);
    }
}

static void ui_refresh_cb(lv_timer_t *t)
{
  scene_type_t s; float conf; char name[24];
  device_state_get_scene(&s, &conf, name, sizeof(name));
  lv_label_set_text(g_lbl_scene, name);
  char cb[32]; snprintf(cb, sizeof(cb), "%.0f%%", conf*100);
  lv_label_set_text(g_lbl_conf, cb);

  anomaly_type_t a = device_state_get_anomaly();
  if (a != ANOMALY_NONE)
    {
      lv_label_set_text(g_lbl_anomaly, anomaly_type_name(a));
      lv_obj_set_style_text_color(g_lbl_anomaly, lv_color_hex(0xD50000), 0);
    }
  else
    {
      lv_label_set_text(g_lbl_anomaly, "");
    }

  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  char tb[16]; snprintf(tb, sizeof(tb), "%02d:%02d", lt->tm_hour, lt->tm_min);
  lv_label_set_text(g_lbl_time, tb);

  update_devices_panel();
}

bool scene_ui_init(void)
{
  lv_obj_t *scr = lv_scr_act();

  g_lbl_scene = lv_label_create(scr);
  lv_obj_set_style_text_font(g_lbl_scene, &lv_font_large, 0);
  lv_label_set_text(g_lbl_scene, "启动中");
  lv_obj_align(g_lbl_scene, LV_ALIGN_TOP_MID, 0, 10);

  g_lbl_conf = lv_label_create(scr);
  lv_obj_align(g_lbl_conf, LV_ALIGN_TOP_MID, 0, 50);

  g_lbl_time = lv_label_create(scr);
  lv_obj_set_style_text_font(g_lbl_time, &lv_font_large, 0);
  lv_label_set_text(g_lbl_time, "--:--");
  lv_obj_align(g_lbl_time, LV_ALIGN_TOP_RIGHT, -10, 10);

  g_lbl_anomaly = lv_label_create(scr);
  lv_obj_set_style_text_font(g_lbl_anomaly, &lv_font_large, 0);
  lv_obj_align(g_lbl_anomaly, LV_ALIGN_TOP_LEFT, 10, 10);

  g_dev_cont = lv_obj_create(scr);
  lv_obj_set_size(g_dev_cont, 460, 160);
  lv_obj_align(g_dev_cont, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_flex_flow(g_dev_cont, LV_FLEX_FLOW_COLUMN);

  lv_timer_create(ui_refresh_cb, 200, NULL);
  syslog(LOG_INFO, "scene UI ready\n");
  return true;
}
