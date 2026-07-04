#include <nuttx/config.h>
#include <string.h>
#include <syslog.h>
#include "display_manager.h"

static display_info_t g_displays[DISPLAY_MAX];
static display_ui_builder_t g_builders[DISPLAY_MAX];
static bool g_inited = false;

int display_manager_init(void)
{
  memset(g_displays, 0, sizeof(g_displays));
  memset(g_builders, 0, sizeof(g_builders));
  g_inited = true;
  return 0;
}

int display_manager_register(display_id_t id, lv_display_t *disp,
                             int32_t w, int32_t h)
{
  if (!g_inited || id < 0 || id >= DISPLAY_MAX) return -1;
  if (w <= 0 || h <= 0) return -1;
  g_displays[id].id = id;
  g_displays[id].disp = disp;
  g_displays[id].width = w;
  g_displays[id].height = h;
  g_displays[id].screen = NULL;
  g_displays[id].active = true;
  syslog(LOG_INFO, "display[%d] registered: %dx%d\n", id, (int)w, (int)h);
  return 0;
}

display_info_t *display_manager_get(display_id_t id)
{
  if (id < 0 || id >= DISPLAY_MAX) return NULL;
  if (!g_displays[id].active) return NULL;
  return &g_displays[id];
}

int display_manager_count(void)
{
  int n = 0;
  int i;
  for (i = 0; i < DISPLAY_MAX; i++)
    if (g_displays[i].active) n++;
  return n;
}

void display_manager_set_ui_builder(display_id_t id, display_ui_builder_t builder)
{
  if (id < 0 || id >= DISPLAY_MAX) return;
  g_builders[id] = builder;
}

void display_manager_build_all(void)
{
  int i;
  for (i = 0; i < DISPLAY_MAX; i++)
    {
      if (!g_displays[i].active || !g_builders[i]) continue;
      if (g_displays[i].disp)
        lv_display_set_active(g_displays[i].disp);
      if (i == 0)
        g_displays[i].screen = lv_scr_act();
      else
        {
          g_displays[i].screen = lv_obj_create(NULL);
          lv_screen_load(g_displays[i].screen);
        }
      g_builders[i](&g_displays[i]);
      syslog(LOG_INFO, "display[%d] UI built\n", i);
    }
}
