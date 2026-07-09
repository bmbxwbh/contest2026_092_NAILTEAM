/****************************************************************************
 * AI Scene-Aware Smart Speaker - LVGL UI
 *
 * 2.8" SPI LCD (240x320) scene visualization.
 *
 * Layout:
 *   ┌─────────────────────────────┐
 *   │     [场景图标] 48x48        │  Header: 0-70
 *   │     场景名称                 │
 *   │     置信度: 85%              │
 *   ├─────────────────────────────┤
 *   │ 🌡️ 25.5°C  💧 50%          │  Sensors: 70-110
 *   │ 🌬️ VOC:120  ☀️ 300lux      │
 *   ├─────────────────────────────┤
 *   │ ┌─ 联动设备 ──────────────┐ │  Devices: 110-260
 *   │ │ ✅ 客厅灯: 开启 70%     │ │
 *   │ │ ✅ 空调: 设为 25°C      │ │
 *   │ │ ○ 电视: 关闭            │ │
 *   │ └────────────────────────┘ │
 *   ├─────────────────────────────┤
 *   │ 🎤 IDLE | 🔊 Vol:50%      │  Status: 260-320
 *   │ ⏱️ 运行: 00:05:23         │
 *   └─────────────────────────────┘
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef CONFIG_GRAPHICS_LVGL
#include <lvgl/lvgl.h>
#endif

#include "scene_aware_speaker.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Screen dimensions (2.8" SPI LCD) */

#define SCREEN_WIDTH            240
#define SCREEN_HEIGHT           320

/* Layout regions */

#define HEADER_Y                0
#define HEADER_HEIGHT           70
#define SENSOR_Y                75
#define SENSOR_HEIGHT           35
#define DEVICE_LIST_Y           115
#define DEVICE_LIST_HEIGHT      145
#define STATUS_Y                265
#define STATUS_HEIGHT           55

/* Margins */

#define MARGIN_X                8
#define MARGIN_Y                5

/* Colors - Dark theme */

#define COLOR_BG                lv_color_hex(0x0D1117)
#define COLOR_BG_HEADER         lv_color_hex(0x161B22)
#define COLOR_BG_SENSOR         lv_color_hex(0x1C2333)
#define COLOR_BG_DEVICE         lv_color_hex(0x21262D)
#define COLOR_BG_STATUS         lv_color_hex(0x161B22)
#define COLOR_TEXT_PRIMARY       lv_color_hex(0xE6EDF3)
#define COLOR_TEXT_SECONDARY     lv_color_hex(0x8B949E)
#define COLOR_ACCENT             lv_color_hex(0x58A6FF)
#define COLOR_SUCCESS            lv_color_hex(0x3FB950)
#define COLOR_WARNING            lv_color_hex(0xD29922)
#define COLOR_ERROR              lv_color_hex(0xF85149)

/* Scene colors */

#define COLOR_SCENE_HOME         lv_color_hex(0x3FB950)  /* Green */
#define COLOR_SCENE_SLEEP        lv_color_hex(0x79C0FF)  /* Light blue */
#define COLOR_SCENE_COOKING      lv_color_hex(0xD29922)  /* Orange */
#define COLOR_SCENE_WORKING      lv_color_hex(0x58A6FF)  /* Blue */
#define COLOR_SCENE_ENTERTAINMENT lv_color_hex(0xBC8CF2) /* Purple */

/* Max devices to display */

#define MAX_DISPLAY_DEVICES      5

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* UI element containers */

typedef struct
{
  /* Header section */

  lv_obj_t *header_panel;
  lv_obj_t *scene_icon;
  lv_obj_t *scene_name_label;
  lv_obj_t *confidence_bar;
  lv_obj_t *confidence_label;

  /* Sensor section */

  lv_obj_t *sensor_panel;
  lv_obj_t *temp_label;
  lv_obj_t *humidity_label;
  lv_obj_t *voc_label;
  lv_obj_t *light_label;

  /* Device list section */

  lv_obj_t *device_panel;
  lv_obj_t *device_title;
  lv_obj_t *device_items[MAX_DISPLAY_DEVICES];
  lv_obj_t *device_icons[MAX_DISPLAY_DEVICES];
  lv_obj_t *device_names[MAX_DISPLAY_DEVICES];
  lv_obj_t *device_states[MAX_DISPLAY_DEVICES];

  /* Status section */

  lv_obj_t *status_panel;
  lv_obj_t *state_label;
  lv_obj_t *volume_label;
  lv_obj_t *uptime_label;
  lv_obj_t *mode_label;
} ui_elements_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_GRAPHICS_LVGL
static ui_elements_t g_ui;

/* Scene information */

static const struct
{
  const char *icon;
  const char *name;
  lv_color_t color;
} g_scene_info[SCENE_COUNT] =
{
  {LV_SYMBOL_WARNING,    "未知场景",  COLOR_TEXT_SECONDARY},
  {LV_SYMBOL_HOME,       "居家模式",  COLOR_SCENE_HOME},
  {LV_SYMBOL_SLEEP,      "睡眠模式",  COLOR_SCENE_SLEEP},
  {LV_SYMBOL_EJECT,      "烹饪模式",  COLOR_SCENE_COOKING},
  {LV_SYMBOL_KEYBOARD,   "工作模式",  COLOR_SCENE_WORKING},
  {LV_SYMBOL_PLAY,       "娱乐模式",  COLOR_SCENE_ENTERTAINMENT}
};

/* Uptime tracking */

static time_t g_start_time = 0;
#endif

static bool g_ui_initialized = false;

/****************************************************************************
 * Private Functions - UI Creation
 ****************************************************************************/

#ifdef CONFIG_GRAPHICS_LVGL

/****************************************************************************
 * Name: create_header_section
 *
 * Description:
 *   Create header section with scene icon and name.
 *
 ****************************************************************************/

static void create_header_section(lv_obj_t *parent)
{
  /* Header panel */

  g_ui.header_panel = lv_obj_create(parent);
  lv_obj_set_size(g_ui.header_panel, SCREEN_WIDTH - 2 * MARGIN_X,
                  HEADER_HEIGHT - 2 * MARGIN_Y);
  lv_obj_set_pos(g_ui.header_panel, MARGIN_X, HEADER_Y + MARGIN_Y);
  lv_obj_set_style_bg_color(g_ui.header_panel, COLOR_BG_HEADER, 0);
  lv_obj_set_style_border_width(g_ui.header_panel, 0, 0);
  lv_obj_set_style_radius(g_ui.header_panel, 8, 0);
  lv_obj_set_style_pad_all(g_ui.header_panel, 8, 0);
  lv_obj_clear_flag(g_ui.header_panel, LV_OBJ_FLAG_SCROLLABLE);

  /* Scene icon (large) */

  g_ui.scene_icon = lv_label_create(g_ui.header_panel);
  lv_label_set_text(g_ui.scene_icon, LV_SYMBOL_AUDIO);
  lv_obj_set_style_text_font(g_ui.scene_icon, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(g_ui.scene_icon, COLOR_ACCENT, 0);
  lv_obj_align(g_ui.scene_icon, LV_ALIGN_LEFT_MID, 0, -8);

  /* Scene name */

  g_ui.scene_name_label = lv_label_create(g_ui.header_panel);
  lv_label_set_text(g_ui.scene_name_label, "初始化中...");
  lv_obj_set_style_text_font(g_ui.scene_name_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(g_ui.scene_name_label, COLOR_TEXT_PRIMARY, 0);
  lv_obj_align(g_ui.scene_name_label, LV_ALIGN_TOP_LEFT, 45, 2);

  /* Confidence bar */

  g_ui.confidence_bar = lv_bar_create(g_ui.header_panel);
  lv_obj_set_size(g_ui.confidence_bar, 120, 8);
  lv_obj_align(g_ui.confidence_bar, LV_ALIGN_BOTTOM_LEFT, 45, -2);
  lv_bar_set_range(g_ui.confidence_bar, 0, 100);
  lv_bar_set_value(g_ui.confidence_bar, 0, LV_ANIM_ON);
  lv_obj_set_style_bg_color(g_ui.confidence_bar,
                            lv_color_hex(0x30363D), LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_ui.confidence_bar,
                            COLOR_SUCCESS, LV_PART_INDICATOR);
  lv_obj_set_style_radius(g_ui.confidence_bar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(g_ui.confidence_bar, 4, LV_PART_INDICATOR);

  /* Confidence label */

  g_ui.confidence_label = lv_label_create(g_ui.header_panel);
  lv_label_set_text(g_ui.confidence_label, "0%");
  lv_obj_set_style_text_font(g_ui.confidence_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_ui.confidence_label, COLOR_TEXT_SECONDARY, 0);
  lv_obj_align(g_ui.confidence_label, LV_ALIGN_BOTTOM_RIGHT, -5, -2);
}

/****************************************************************************
 * Name: create_sensor_section
 *
 * Description:
 *   Create sensor data display section.
 *
 ****************************************************************************/

static void create_sensor_section(lv_obj_t *parent)
{
  /* Sensor panel */

  g_ui.sensor_panel = lv_obj_create(parent);
  lv_obj_set_size(g_ui.sensor_panel, SCREEN_WIDTH - 2 * MARGIN_X,
                  SENSOR_HEIGHT - MARGIN_Y);
  lv_obj_set_pos(g_ui.sensor_panel, MARGIN_X, SENSOR_Y);
  lv_obj_set_style_bg_color(g_ui.sensor_panel, COLOR_BG_SENSOR, 0);
  lv_obj_set_style_border_width(g_ui.sensor_panel, 0, 0);
  lv_obj_set_style_radius(g_ui.sensor_panel, 8, 0);
  lv_obj_set_style_pad_all(g_ui.sensor_panel, 6, 0);
  lv_obj_clear_flag(g_ui.sensor_panel, LV_OBJ_FLAG_SCROLLABLE);

  /* Temperature */

  g_ui.temp_label = lv_label_create(g_ui.sensor_panel);
  lv_label_set_text(g_ui.temp_label, LV_SYMBOL_HEAT " --°C");
  lv_obj_set_style_text_font(g_ui.temp_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_ui.temp_label, COLOR_TEXT_PRIMARY, 0);
  lv_obj_align(g_ui.temp_label, LV_ALIGN_LEFT_MID, 0, 0);

  /* Humidity */

  g_ui.humidity_label = lv_label_create(g_ui.sensor_panel);
  lv_label_set_text(g_ui.humidity_label, LV_SYMBOL_DROPLET " --%");
  lv_obj_set_style_text_font(g_ui.humidity_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_ui.humidity_label, COLOR_TEXT_PRIMARY, 0);
  lv_obj_align(g_ui.humidity_label, LV_ALIGN_CENTER, 0, 0);

  /* VOC */

  g_ui.voc_label = lv_label_create(g_ui.sensor_panel);
  lv_label_set_text(g_ui.voc_label, LV_SYMBOL_WARNING " --");
  lv_obj_set_style_text_font(g_ui.voc_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_ui.voc_label, COLOR_TEXT_PRIMARY, 0);
  lv_obj_align(g_ui.voc_label, LV_ALIGN_RIGHT_MID, 0, 0);

  /* Light (hidden, shown in second row if needed) */

  g_ui.light_label = lv_label_create(g_ui.sensor_panel);
  lv_label_set_text(g_ui.light_label, LV_SYMBOL_EYE_OPEN " -- lux");
  lv_obj_set_style_text_font(g_ui.light_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_ui.light_label, COLOR_TEXT_PRIMARY, 0);
  lv_obj_add_flag(g_ui.light_label, LV_OBJ_FLAG_HIDDEN);
}

/****************************************************************************
 * Name: create_device_list_section
 *
 * Description:
 *   Create device list section with status indicators.
 *
 ****************************************************************************/

static void create_device_list_section(lv_obj_t *parent)
{
  /* Device panel */

  g_ui.device_panel = lv_obj_create(parent);
  lv_obj_set_size(g_ui.device_panel, SCREEN_WIDTH - 2 * MARGIN_X,
                  DEVICE_LIST_HEIGHT);
  lv_obj_set_pos(g_ui.device_panel, MARGIN_X, DEVICE_LIST_Y);
  lv_obj_set_style_bg_color(g_ui.device_panel, COLOR_BG_DEVICE, 0);
  lv_obj_set_style_border_width(g_ui.device_panel, 0, 0);
  lv_obj_set_style_radius(g_ui.device_panel, 8, 0);
  lv_obj_set_style_pad_all(g_ui.device_panel, 8, 0);
  lv_obj_clear_flag(g_ui.device_panel, LV_OBJ_FLAG_SCROLLABLE);

  /* Title */

  g_ui.device_title = lv_label_create(g_ui.device_panel);
  lv_label_set_text(g_ui.device_title, LV_SYMBOL_HOME " 联动设备");
  lv_obj_set_style_text_font(g_ui.device_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_ui.device_title, COLOR_TEXT_SECONDARY, 0);
  lv_obj_align(g_ui.device_title, LV_ALIGN_TOP_LEFT, 0, 0);

  /* Device items */

  for (int i = 0; i < MAX_DISPLAY_DEVICES; i++)
    {
      int y_offset = 22 + i * 22;

      /* Status icon */

      g_ui.device_icons[i] = lv_label_create(g_ui.device_panel);
      lv_label_set_text(g_ui.device_icons[i], LV_SYMBOL_CLOSE);
      lv_obj_set_style_text_font(g_ui.device_icons[i],
                                 &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(g_ui.device_icons[i],
                                  COLOR_TEXT_SECONDARY, 0);
      lv_obj_align(g_ui.device_icons[i], LV_ALIGN_TOP_LEFT, 0, y_offset);

      /* Device name */

      g_ui.device_names[i] = lv_label_create(g_ui.device_panel);
      lv_label_set_text(g_ui.device_names[i], "--");
      lv_obj_set_style_text_font(g_ui.device_names[i],
                                 &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(g_ui.device_names[i],
                                  COLOR_TEXT_PRIMARY, 0);
      lv_obj_align(g_ui.device_names[i], LV_ALIGN_TOP_LEFT, 18, y_offset);

      /* Device state */

      g_ui.device_states[i] = lv_label_create(g_ui.device_panel);
      lv_label_set_text(g_ui.device_states[i], "关闭");
      lv_obj_set_style_text_font(g_ui.device_states[i],
                                 &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(g_ui.device_states[i],
                                  COLOR_TEXT_SECONDARY, 0);
      lv_obj_align(g_ui.device_states[i], LV_ALIGN_TOP_RIGHT, 0, y_offset);
    }
}

/****************************************************************************
 * Name: create_status_section
 *
 * Description:
 *   Create status bar at bottom of screen.
 *
 ****************************************************************************/

static void create_status_section(lv_obj_t *parent)
{
  /* Status panel */

  g_ui.status_panel = lv_obj_create(parent);
  lv_obj_set_size(g_ui.status_panel, SCREEN_WIDTH - 2 * MARGIN_X,
                  STATUS_HEIGHT - MARGIN_Y);
  lv_obj_set_pos(g_ui.status_panel, MARGIN_X, STATUS_Y);
  lv_obj_set_style_bg_color(g_ui.status_panel, COLOR_BG_STATUS, 0);
  lv_obj_set_style_border_width(g_ui.status_panel, 0, 0);
  lv_obj_set_style_radius(g_ui.status_panel, 8, 0);
  lv_obj_set_style_pad_all(g_ui.status_panel, 8, 0);
  lv_obj_clear_flag(g_ui.status_panel, LV_OBJ_FLAG_SCROLLABLE);

  /* State icon + label */

  g_ui.state_label = lv_label_create(g_ui.status_panel);
  lv_label_set_text(g_ui.state_label, LV_SYMBOL_AUDIO " 待机");
  lv_obj_set_style_text_font(g_ui.state_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_ui.state_label, COLOR_SUCCESS, 0);
  lv_obj_align(g_ui.state_label, LV_ALIGN_TOP_LEFT, 0, 0);

  /* Volume */

  g_ui.volume_label = lv_label_create(g_ui.status_panel);
  lv_label_set_text(g_ui.volume_label, LV_SYMBOL_VOLUME_MAX " 50%");
  lv_obj_set_style_text_font(g_ui.volume_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_ui.volume_label, COLOR_TEXT_SECONDARY, 0);
  lv_obj_align(g_ui.volume_label, LV_ALIGN_TOP_RIGHT, 0, 0);

  /* Mode */

  g_ui.mode_label = lv_label_create(g_ui.status_panel);
  lv_label_set_text(g_ui.mode_label, "本地AI模式");
  lv_obj_set_style_text_font(g_ui.mode_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_ui.mode_label, COLOR_ACCENT, 0);
  lv_obj_align(g_ui.mode_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  /* Uptime */

  g_ui.uptime_label = lv_label_create(g_ui.status_panel);
  lv_label_set_text(g_ui.uptime_label, "00:00:00");
  lv_obj_set_style_text_font(g_ui.uptime_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_ui.uptime_label, COLOR_TEXT_SECONDARY, 0);
  lv_obj_align(g_ui.uptime_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

/****************************************************************************
 * Name: update_uptime
 *
 * Description:
 *   Update uptime display.
 *
 ****************************************************************************/

static void update_uptime(void)
{
  if (!g_ui.uptime_label || g_start_time == 0)
    {
      return;
    }

  time_t now;
  time(&now);
  int elapsed = (int)difftime(now, g_start_time);

  int hours = elapsed / 3600;
  int minutes = (elapsed % 3600) / 60;
  int seconds = elapsed % 60;

  lv_label_set_text_fmt(g_ui.uptime_label, "%02d:%02d:%02d",
                        hours, minutes, seconds);
}

#endif /* CONFIG_GRAPHICS_LVGL */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lvgl_ui_init
 *
 * Description:
 *   Initialize LVGL UI with complete layout.
 *
 ****************************************************************************/

int lvgl_ui_init(void)
{
#ifdef CONFIG_GRAPHICS_LVGL
  lv_obj_t *scr = lv_scr_act();

  /* Set screen background */

  lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  /* Create UI sections */

  create_header_section(scr);
  create_sensor_section(scr);
  create_device_list_section(scr);
  create_status_section(scr);

  /* Record start time */

  time(&g_start_time);

  g_ui_initialized = true;
  printf("[UI] LVGL UI initialized (240x320)\n");
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
  if (!result || !g_ui_initialized)
    {
      return -EINVAL;
    }

  const char *scene_name = scene_type_to_string(result->scene);
  int confidence_pct = (int)(result->confidence * 100);

#ifdef CONFIG_GRAPHICS_LVGL
  if (result->scene >= 0 && result->scene < SCENE_COUNT)
    {
      /* Update scene icon */

      if (g_ui.scene_icon)
        {
          lv_label_set_text(g_ui.scene_icon,
                           g_scene_info[result->scene].icon);
          lv_obj_set_style_text_color(g_ui.scene_icon,
                                     g_scene_info[result->scene].color, 0);
        }

      /* Update scene name */

      if (g_ui.scene_name_label)
        {
          lv_label_set_text(g_ui.scene_name_label,
                           g_scene_info[result->scene].name);
        }
    }

  /* Update confidence bar */

  if (g_ui.confidence_bar)
    {
      lv_bar_set_value(g_ui.confidence_bar, confidence_pct, LV_ANIM_ON);

      /* Color based on confidence */

      if (confidence_pct >= 80)
        {
          lv_obj_set_style_bg_color(g_ui.confidence_bar,
                                   COLOR_SUCCESS, LV_PART_INDICATOR);
        }
      else if (confidence_pct >= 50)
        {
          lv_obj_set_style_bg_color(g_ui.confidence_bar,
                                   COLOR_WARNING, LV_PART_INDICATOR);
        }
      else
        {
          lv_obj_set_style_bg_color(g_ui.confidence_bar,
                                   COLOR_ERROR, LV_PART_INDICATOR);
        }
    }

  /* Update confidence label */

  if (g_ui.confidence_label)
    {
      lv_label_set_text_fmt(g_ui.confidence_label, "%d%%", confidence_pct);
    }

  /* Update sensor data */

  if (result->sensor_data.valid)
    {
      if (g_ui.temp_label)
        {
          lv_label_set_text_fmt(g_ui.temp_label,
                               LV_SYMBOL_HEAT " %.1f°C",
                               result->sensor_data.temperature);
        }

      if (g_ui.humidity_label)
        {
          lv_label_set_text_fmt(g_ui.humidity_label,
                               LV_SYMBOL_DROPLET " %.0f%%",
                               result->sensor_data.humidity);
        }

      if (g_ui.voc_label)
        {
          lv_label_set_text_fmt(g_ui.voc_label,
                               LV_SYMBOL_WARNING " %d",
                               result->sensor_data.tvoc);
        }
    }

  /* Update uptime */

  update_uptime();

#else
  printf("[UI] Scene: %s (%d%%)\n", scene_name, confidence_pct);

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
 *   Update device list with agent decision.
 *
 ****************************************************************************/

int lvgl_ui_update_actions(const agent_decision_t *decision)
{
  if (!decision || !g_ui_initialized)
    {
      return -EINVAL;
    }

#ifdef CONFIG_GRAPHICS_LVGL
  /* Update device list */

  for (int i = 0; i < MAX_DISPLAY_DEVICES; i++)
    {
      if (i < decision->num_actions)
        {
          const mihome_action_t *action = &decision->actions[i];

          /* Show device */

          if (g_ui.device_icons[i])
            {
              lv_label_set_text(g_ui.device_icons[i],
                               LV_SYMBOL_OK);
              lv_obj_set_style_text_color(g_ui.device_icons[i],
                                         COLOR_SUCCESS, 0);
              lv_obj_clear_flag(g_ui.device_icons[i], LV_OBJ_FLAG_HIDDEN);
            }

          if (g_ui.device_names[i])
            {
              lv_label_set_text(g_ui.device_names[i], action->device_name);
              lv_obj_clear_flag(g_ui.device_names[i], LV_OBJ_FLAG_HIDDEN);
            }

          if (g_ui.device_states[i])
            {
              lv_label_set_text(g_ui.device_states[i], action->action_desc);
              lv_obj_set_style_text_color(g_ui.device_states[i],
                                         COLOR_SUCCESS, 0);
              lv_obj_clear_flag(g_ui.device_states[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
      else
        {
          /* Hide unused slots */

          if (g_ui.device_icons[i])
            {
              lv_obj_add_flag(g_ui.device_icons[i], LV_OBJ_FLAG_HIDDEN);
            }

          if (g_ui.device_names[i])
            {
              lv_obj_add_flag(g_ui.device_names[i], LV_OBJ_FLAG_HIDDEN);
            }

          if (g_ui.device_states[i])
            {
              lv_obj_add_flag(g_ui.device_states[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

  /* Update title with action count */

  if (g_ui.device_title)
    {
      lv_label_set_text_fmt(g_ui.device_title,
                           LV_SYMBOL_HOME " 联动设备 (%d)",
                           decision->num_actions);
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
  if (!g_ui_initialized)
    {
      return -EINVAL;
    }

  const char *state_name = app_state_to_string(state);

#ifdef CONFIG_GRAPHICS_LVGL
  if (g_ui.state_label)
    {
      const char *icon;
      lv_color_t color;

      switch (state)
        {
          case APP_STATE_IDLE:
            icon = LV_SYMBOL_AUDIO;
            color = COLOR_SUCCESS;
            break;
          case APP_STATE_LISTENING:
            icon = LV_SYMBOL_EYE_OPEN;
            color = COLOR_ACCENT;
            break;
          case APP_STATE_PROCESSING:
            icon = LV_SYMBOL_REFRESH;
            color = COLOR_WARNING;
            break;
          case APP_STATE_RESPONDING:
            LV_SYMBOL_SPEAKER;
            color = COLOR_SUCCESS;
            break;
          case APP_STATE_SCENE_DETECT:
            icon = LV_SYMBOL_CHARGE;
            color = COLOR_ACCENT;
            break;
          default:
            icon = LV_SYMBOL_WARNING;
            color = COLOR_TEXT_SECONDARY;
            break;
        }

      lv_label_set_text_fmt(g_ui.state_label, "%s %s", icon, state_name);
      lv_obj_set_style_text_color(g_ui.state_label, color, 0);
    }
#else
  printf("[UI] State: %s\n", state_name);
#endif

  return 0;
}

/****************************************************************************
 * Name: lvgl_ui_update_volume
 *
 * Description:
 *   Update volume display.
 *
 ****************************************************************************/

int lvgl_ui_update_volume(int volume_pct)
{
  if (!g_ui_initialized)
    {
      return -EINVAL;
    }

#ifdef CONFIG_GRAPHICS_LVGL
  if (g_ui.volume_label)
    {
      const char *icon;

      if (volume_pct == 0)
        {
          icon = LV_SYMBOL_MUTE;
        }
      else if (volume_pct < 30)
        {
          icon = LV_SYMBOL_VOLUME_MID;
        }
      else
        {
          icon = LV_SYMBOL_VOLUME_MAX;
        }

      lv_label_set_text_fmt(g_ui.volume_label, "%s %d%%",
                           icon, volume_pct);
    }
#else
  printf("[UI] Volume: %d%%\n", volume_pct);
#endif

  return 0;
}

/****************************************************************************
 * Name: lvgl_ui_show_alert
 *
 * Description:
 *   Show alert message on screen.
 *
 ****************************************************************************/

int lvgl_ui_show_alert(const char *title, const char *message,
                       lv_color_t color)
{
  if (!title || !message || !g_ui_initialized)
    {
      return -EINVAL;
    }

#ifdef CONFIG_GRAPHICS_LVGL
  /* Create alert overlay */

  lv_obj_t *alert = lv_obj_create(lv_scr_act());
  lv_obj_set_size(alert, SCREEN_WIDTH - 40, 80);
  lv_obj_center(alert);
  lv_obj_set_style_bg_color(alert, COLOR_BG_HEADER, 0);
  lv_obj_set_style_border_color(alert, color, 0);
  lv_obj_set_style_border_width(alert, 2, 0);
  lv_obj_set_style_radius(alert, 12, 0);
  lv_obj_set_style_pad_all(alert, 12, 0);

  /* Title */

  lv_obj_t *title_label = lv_label_create(alert);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title_label, color, 0);
  lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

  /* Message */

  lv_obj_t *msg_label = lv_label_create(alert);
  lv_label_set_text(msg_label, message);
  lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(msg_label, COLOR_TEXT_PRIMARY, 0);
  lv_obj_align(msg_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  /* Auto-dismiss after 3 seconds */

  lv_obj_add_event_cb(alert, NULL, LV_EVENT_CLICKED, NULL);

  printf("[UI] Alert: %s - %s\n", title, message);
#else
  printf("[UI] Alert: %s - %s\n", title, message);
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
  /* Delete all UI objects */

  if (g_ui.header_panel)
    {
      lv_obj_del(g_ui.header_panel);
    }

  if (g_ui.sensor_panel)
    {
      lv_obj_del(g_ui.sensor_panel);
    }

  if (g_ui.device_panel)
    {
      lv_obj_del(g_ui.device_panel);
    }

  if (g_ui.status_panel)
    {
      lv_obj_del(g_ui.status_panel);
    }

  memset(&g_ui, 0, sizeof(g_ui));
#endif

  g_ui_initialized = false;
  g_start_time = 0;
  printf("[UI] LVGL UI cleaned up\n");
}
