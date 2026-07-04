#ifndef __APPS_SMART_SPEAKER_INCLUDE_DISPLAY_MANAGER_H
#define __APPS_SMART_SPEAKER_INCLUDE_DISPLAY_MANAGER_H

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include "lvgl/lvgl.h"

#define DISPLAY_MAX  2

typedef enum
{
  DISPLAY_PRIMARY = 0,
  DISPLAY_SECONDARY = 1,
} display_id_t;

typedef struct
{
  display_id_t  id;
  int32_t       width;
  int32_t       height;
  lv_display_t *disp;
  lv_obj_t     *screen;
  bool          active;
} display_info_t;

typedef void (*display_ui_builder_t)(display_info_t *info);

int  display_manager_init(void);
int  display_manager_register(display_id_t id, lv_display_t *disp,
                              int32_t w, int32_t h);
display_info_t *display_manager_get(display_id_t id);
int  display_manager_count(void);
void display_manager_set_ui_builder(display_id_t id, display_ui_builder_t builder);
void display_manager_build_all(void);

#endif
