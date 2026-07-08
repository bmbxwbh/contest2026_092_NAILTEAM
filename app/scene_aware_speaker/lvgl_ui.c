/****************************************************************************
 * AI Scene-Aware Smart Speaker - LVGL UI
 *
 * Displays scene status, sensor data, and device actions on LCD.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_GRAPHICS_LVGL
#include <lvgl/lvgl.h>
#endif

#include "scene_aware_speaker.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UI_SCENE_LABEL_Y        10
#define UI_STATE_LABEL_Y        35
#define UI_SENSOR_LABEL_Y       60
#define UI_ACTION_LABEL_Y       100
#define UI_MARGIN               10

#define UI_COLOR_SCENE_BG       lv_color_hex(0x1a1a2e)
#define UI_COLOR_SCENE_TEXT     lv_color_hex(0xeaeaea)
#define UI_COLOR_ACTION_BG      lv_color_hex(0x16213e)
#define UI_COLOR_ACTION_TEXT    lv_color_hex(0x0f3460)

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_GRAPHICS_LVGL
static lv_obj_t *g_scene_label = NULL;
static lv_obj_t *g_state_label = NULL;
static lv_obj_t *g_sensor_label = NULL;
static lv_obj_t *g_action_label = NULL;
static lv_obj_t *g_scene_icon = NULL;

/* Scene icons (emoji-like symbols) */

static const char *g_scene_icons[SCENE_COUNT] =
{
  "?",      /* SCENE_UNKNOWN */
  LV_SYMBOL_HOME,      /* SCENE_HOME */
  LV_SYMBOL_SLEEP,     /* SCENE_SLEEP */
  LV_SYMBOL_COOKING,   /* SCENE_COOKING - using custom if available */
  LV_SYMBOL_WORK,      /* SCENE_WORKING - using custom if available */
  LV_SYMBOL_PLAY       /* SCENE_ENTERTAINMENT */
};
#endif

static bool g_ui_initialized = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_scene_color
 *
 * Description:
 *   Get color for scene type.
 *
 ****************************************************************************/

#ifdef CONFIG_GRAPHICS_LVGL
static lv_color_t get_scene_color(scene_type_t scene)
{
  switch (scene)
    {
      case SCENE_HOME:
        return lv_color_hex(0x4CAF50);  /* Green */
      case SCENE_SLEEP:
        return lv_color_hex(0x3F51B5);  /* Indigo */
      case SCENE_COOKING:
        return lv_color_hex(0xFF9800);  /* Orange */
      case SCENE_WORKING:
        return lv_color_hex(0x2196F3);  /* Blue */
      case SCENE_ENTERTAINMENT:
        return lv_color_hex(0x9C27B0);  /* Purple */
      default:
        return lv_color_hex(0x9E9E9E);  /* Grey */
    }
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lvgl_ui_init
 *
 * Description:
 *   Initialize LVGL UI components.
 *
 ****************************************************************************/

int lvgl_ui_init(void)
{
#ifdef CONFIG_GRAPHICS_LVGL
  lv_obj_t *scr = lv_scr_act();

  /* Set screen background */

  lv_obj_set_style_bg_color(scr, UI_COLOR_SCENE_BG, 0);

  /* Scene icon */

  g_scene_icon = lv_label_create(scr);
  lv_label_set_text(g_scene_icon, LV_SYMBOL_AUDIO);
  lv_obj_set_style_text_font(g_scene_icon, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(g_scene_icon, get_scene_color(SCENE_UNKNOWN), 0);
  lv_obj_align(g_scene_icon, LV_ALIGN_TOP_MID, 0, UI_MARGIN);

  /* Scene label */

  g_scene_label = lv_label_create(scr);
  lv_label_set_text(g_scene_label, "Scene: Initializing...");
  lv_obj_set_style_text_font(g_scene_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(g_scene_label, UI_COLOR_SCENE_TEXT, 0);
  lv_obj_align(g_scene_label, LV_ALIGN_TOP_LEFT, UI_MARGIN, UI_SCENE_LABEL_Y);

  /* State label */

  g_state_label = lv_label_create(scr);
  lv_label_set_text(g_state_label, "State: IDLE");
  lv_obj_set_style_text_font(g_state_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(g_state_label, UI_COLOR_SCENE_TEXT, 0);
  lv_obj_align(g_state_label, LV_ALIGN_TOP_LEFT, UI_MARGIN, UI_STATE_LABEL_Y);

  /* Sensor label */

  g_sensor_label = lv_label_create(scr);
  lv_label_set_text(g_sensor_label, "Sensors: --");
  lv_obj_set_style_text_font(g_sensor_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(g_sensor_label, UI_COLOR_SCENE_TEXT, 0);
  lv_obj_align(g_sensor_label, LV_ALIGN_TOP_LEFT, UI_MARGIN, UI_SENSOR_LABEL_Y);

  /* Action label */

  g_action_label = lv_label_create(scr);
  lv_label_set_text(g_action_label, "Actions: None");
  lv_obj_set_style_text_font(g_action_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(g_action_label, UI_COLOR_SCENE_TEXT, 0);
  lv_obj_set_width(g_action_label, 200);
  lv_label_set_long_mode(g_action_label, LV_LABEL_LONG_WRAP);
  lv_obj_align(g_action_label, LV_ALIGN_TOP_LEFT, UI_MARGIN, UI_ACTION_LABEL_Y);

  g_ui_initialized = true;
  printf("[UI] LVGL UI initialized\n");
#else
  printf("[UI] LVGL not available, using serial output\n");
  g_ui_initialized = true;
#endif

  return 0;
}

/****************************************************************************
 * Name: lvgl_ui_update_scene
 *
 * Description:
 *   Update scene display with new detection result.
 *
 ****************************************************************************/

int lvgl_ui_update_scene(const scene_result_t *result)
{
  if (!result)
    {
      return -EINVAL;
    }

  const char *scene_name = scene_type_to_string(result->scene);

#ifdef CONFIG_GRAPHICS_LVGL
  if (g_scene_label && g_scene_icon)
    {
      /* Update scene text */

      lv_label_set_text_fmt(g_scene_label, "Scene: %s (%.0f%%)",
                            scene_name, result->confidence * 100);

      /* Update scene icon color */

      lv_obj_set_style_text_color(g_scene_icon,
                                  get_scene_color(result->scene), 0);

      /* Update icon based on scene */

      if (result->scene < SCENE_COUNT)
        {
          lv_label_set_text(g_scene_icon, g_scene_icons[result->scene]);
        }
    }

  /* Update sensor data display */

  if (g_sensor_label && result->sensor_data.valid)
    {
      lv_label_set_text_fmt(g_sensor_label,
                            "T:%.1fC H:%.0f%% VOC:%d Light:%d",
                            result->sensor_data.temperature,
                            result->sensor_data.humidity,
                            result->sensor_data.tvoc,
                            result->sensor_data.light);
    }
#else
  printf("[UI] Scene: %s (%.0f%%)\n", scene_name,
         result->confidence * 100);

  if (result->sensor_data.valid)
    {
      printf("[UI] Sensors: T=%.1fC H=%.0f%% VOC=%d Light=%d\n",
             result->sensor_data.temperature,
             result->sensor_data.humidity,
             result->sensor_data.tvoc,
             result->sensor_data.light);
    }
#endif

  return 0;
}

/****************************************************************************
 * Name: lvgl_ui_update_actions
 *
 * Description:
 *   Update action display with agent decision.
 *
 ****************************************************************************/

int lvgl_ui_update_actions(const agent_decision_t *decision)
{
  if (!decision)
    {
      return -EINVAL;
    }

#ifdef CONFIG_GRAPHICS_LVGL
  if (g_action_label)
    {
      if (decision->num_actions > 0)
        {
          /* Build action string */

          char action_buf[256];
          int pos = 0;

          pos += snprintf(action_buf + pos, sizeof(action_buf) - pos,
                          "Actions:\n");

          for (int i = 0; i < decision->num_actions && pos < sizeof(action_buf); i++)
            {
              pos += snprintf(action_buf + pos, sizeof(action_buf) - pos,
                              "  %s: %s\n",
                              decision->actions[i].device_name,
                              decision->actions[i].action_desc);
            }

          lv_label_set_text(g_action_label, action_buf);
        }
      else
        {
          lv_label_set_text(g_action_label, "Actions: None");
        }
    }
#else
  printf("[UI] Actions: %s\n", decision->description);

  for (int i = 0; i < decision->num_actions; i++)
    {
      printf("[UI]   %s -> %s\n",
             decision->actions[i].device_name,
             decision->actions[i].action_desc);
    }
#endif

  return 0;
}

/****************************************************************************
 * Name: lvgl_ui_update_state
 *
 * Description:
 *   Update application state display.
 *
 ****************************************************************************/

int lvgl_ui_update_state(app_state_t state)
{
  const char *state_name = app_state_to_string(state);

#ifdef CONFIG_GRAPHICS_LVGL
  if (g_state_label)
    {
      lv_label_set_text_fmt(g_state_label, "State: %s", state_name);
    }
#else
  printf("[UI] State: %s\n", state_name);
#endif

  return 0;
}

/****************************************************************************
 * Name: lvgl_ui_cleanup
 *
 * Description:
 *   Cleanup LVGL UI.
 *
 ****************************************************************************/

void lvgl_ui_cleanup(void)
{
#ifdef CONFIG_GRAPHICS_LVGL
  /* LVGL objects are cleaned up automatically */

  g_scene_label = NULL;
  g_state_label = NULL;
  g_sensor_label = NULL;
  g_action_label = NULL;
  g_scene_icon = NULL;
#endif

  g_ui_initialized = false;
  printf("[UI] LVGL UI cleaned up\n");
}
