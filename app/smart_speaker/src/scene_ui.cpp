#include <nuttx/config.h>
#include <stdio.h>
#include <time.h>
#include <syslog.h>
#include "lvgl/lvgl.h"
#include "scene_ui.h"
#include "display_manager.h"
#include "device_state.h"

#define UI_REFRESH_MS  200

typedef struct
{
  lv_obj_t *lbl_scene;
  lv_obj_t *lbl_conf;
  lv_obj_t *lbl_time;
  lv_obj_t *lbl_anomaly;
  lv_obj_t *dev_cont;
  int32_t  disp_w;
  int32_t  disp_h;
} ui_ctx_t;

static ui_ctx_t g_primary;

static void build_layout(display_info_t *info, ui_ctx_t *ctx)
{
  lv_obj_t *scr = info->screen;
  int32_t w = info->width;
  int32_t h = info->height;
  ctx->disp_w = w;
  ctx->disp_h = h;

  ctx->lbl_anomaly = lv_label_create(scr);
  lv_obj_set_style_text_font(ctx->lbl_anomaly, &lv_font_large, 0);
  lv_obj_align(ctx->lbl_anomaly, LV_ALIGN_TOP_LEFT, w / 20, h / 20);

  ctx->lbl_time = lv_label_create(scr);
  lv_obj_set_style_text_font(ctx->lbl_time, &lv_font_large, 0);
  lv_label_set_text(ctx->lbl_time, "--:--");
  lv_obj_align(ctx->lbl_time, LV_ALIGN_TOP_RIGHT, -w / 20, h / 20);

  ctx->lbl_scene = lv_label_create(scr);
  lv_obj_set_style_text_font(ctx->lbl_scene, &lv_font_large, 0);
  lv_label_set_text(ctx->lbl_scene, "启动中");
  lv_obj_align(ctx->lbl_scene, LV_ALIGN_TOP_MID, 0, h / 15);

  ctx->lbl_conf = lv_label_create(scr);
  lv_obj_align(ctx->lbl_conf, LV_ALIGN_TOP_MID, 0, h / 6);

  ctx->dev_cont = lv_obj_create(scr);
  lv_obj_set_size(ctx->dev_cont, w * 19 / 20, h / 2);
  lv_obj_align(ctx->dev_cont, LV_ALIGN_BOTTOM_MID, 0, -h / 30);
  lv_obj_set_flex_flow(ctx->dev_cont, LV_FLEX_FLOW_COLUMN);
}

static void update_devices_panel(ui_ctx_t *ctx)
{
  if (!ctx->dev_cont) return;
  lv_obj_clean(ctx->dev_cont);
  device_entry_t devs[MAX_DEVICES];
  int total = 0;
  int n = device_state_get_devices(devs, MAX_DEVICES, &total);
  int i;
  for (i = 0; i < n; i++)
    {
      char buf[64];
      snprintf(buf, sizeof(buf), "%s: %s %d%%",
               devs[i].name,
               devs[i].on ? "ON" : "off",
               (int)devs[i].brightness);
      lv_obj_t *lbl = lv_label_create(ctx->dev_cont);
      lv_label_set_text(lbl, buf);
      lv_obj_set_style_text_color(lbl,
        devs[i].on ? lv_color_hex(0x00C853) : lv_color_hex(0x9E9E9E), 0);
    }
}

static void ui_refresh_cb(lv_timer_t *t)
{
  ui_ctx_t *ctx = (ui_ctx_t *)t->user_data;
  if (!ctx || !ctx->lbl_scene) return;

  scene_type_t s; float conf; char name[24];
  device_state_get_scene(&s, &conf, name, sizeof(name));
  lv_label_set_text(ctx->lbl_scene, name);
  char cb[32]; snprintf(cb, sizeof(cb), "%.0f%%", conf * 100);
  lv_label_set_text(ctx->lbl_conf, cb);

  anomaly_type_t a = device_state_get_anomaly();
  if (a != ANOMALY_NONE)
    {
      lv_label_set_text(ctx->lbl_anomaly, anomaly_type_name(a));
      lv_obj_set_style_text_color(ctx->lbl_anomaly, lv_color_hex(0xD50000), 0);
    }
  else
    {
      lv_label_set_text(ctx->lbl_anomaly, "");
    }

  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  char tb[16]; snprintf(tb, sizeof(tb), "%02d:%02d", lt->tm_hour, lt->tm_min);
  lv_label_set_text(ctx->lbl_time, tb);

  update_devices_panel(ctx);
}

void scene_ui_build_primary(display_info_t *info)
{
  if (!info || !info->screen) return;
  memset(&g_primary, 0, sizeof(g_primary));
  build_layout(info, &g_primary);
  lv_timer_create(ui_refresh_cb, UI_REFRESH_MS, &g_primary);
  syslog(LOG_INFO, "primary UI built (%dx%d)\n", (int)info->width, (int)info->height);
}

bool scene_ui_init(void)
{
  display_manager_set_ui_builder(DISPLAY_PRIMARY, scene_ui_build_primary);
  display_manager_build_all();
  return true;
}
