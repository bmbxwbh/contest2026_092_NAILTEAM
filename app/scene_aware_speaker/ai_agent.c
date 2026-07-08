/****************************************************************************
 * AI Scene-Aware Smart Speaker - AI Agent Decision Engine
 *
 * Maps detected scenes to smart home actions.
 * Uses rule-based decisions with sensor context.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene_aware_speaker.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Decision rule structures */

#define MAX_ACTIONS_PER_SCENE   8
#define DECISION_DESC_LEN       128

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Scene-to-action mapping rules */

static const agent_decision_t g_scene_rules[SCENE_COUNT] =
{
  /* SCENE_UNKNOWN - no actions */

  [SCENE_UNKNOWN] =
    {
      .scene = SCENE_UNKNOWN,
      .num_actions = 0,
      .description = "Unknown scene - no automation"
    },

  /* SCENE_HOME - normal home environment */

  [SCENE_HOME] =
    {
      .scene = SCENE_HOME,
      .num_actions = 3,
      .actions =
        {
          {
            .device_id = "light_living",
            .device_name = "客厅灯",
            .action = "on_brightness_70",
            .action_desc = "开启客厅灯 70%"
          },
          {
            .device_id = "ac_living",
            .device_name = "客厅空调",
            .action = "set_temp_25",
            .action_desc = "空调设为 25°C"
          },
          {
            .device_id = "curtain_living",
            .device_name = "客厅窗帘",
            .action = "open",
            .action_desc = "打开窗帘"
          }
        },
      .description = "居家模式：灯光 70%，空调 25°C"
    },

  /* SCENE_SLEEP - very quiet, night time */

  [SCENE_SLEEP] =
    {
      .scene = SCENE_SLEEP,
      .num_actions = 4,
      .actions =
        {
          {
            .device_id = "light_bedroom",
            .device_name = "卧室灯",
            .action = "off",
            .action_desc = "关闭卧室灯"
          },
          {
            .device_id = "ac_bedroom",
            .device_name = "卧室空调",
            .action = "set_temp_26",
            .action_desc = "空调设为 26°C"
          },
          {
            .device_id = "curtain_bedroom",
            .device_name = "卧室窗帘",
            .action = "close",
            .action_desc = "关闭窗帘"
          },
          {
            .device_id = "speaker",
            .device_name = "音箱",
            .action = "set_volume_20",
            .action_desc = "音量降至 20%"
          }
        },
      .description = "睡眠模式：关灯，空调 26°C，静音"
    },

  /* SCENE_COOKING - kitchen noise, high temp/VOC */

  [SCENE_COOKING] =
    {
      .scene = SCENE_COOKING,
      .num_actions = 3,
      .actions =
        {
          {
            .device_id = "light_kitchen",
            .device_name = "厨房灯",
            .action = "on_brightness_100",
            .action_desc = "开启厨房灯 100%"
          },
          {
            .device_id = "fan_kitchen",
            .device_name = "油烟机",
            .action = "on_speed_high",
            .action_desc = "开启油烟机高速"
          },
          {
            .device_id = "hood_kitchen",
            .device_name = "厨房排气扇",
            .action = "on",
            .action_desc = "开启排气扇"
          }
        },
      .description = "烹饪模式：厨房灯全亮，油烟机高速"
    },

  /* SCENE_WORKING - keyboard/whisper, focus mode */

  [SCENE_WORKING] =
    {
      .scene = SCENE_WORKING,
      .num_actions = 3,
      .actions =
        {
          {
            .device_id = "light_study",
            .device_name = "书房灯",
            .action = "on_brightness_100",
            .action_desc = "开启书房灯 100%"
          },
          {
            .device_id = "ac_study",
            .device_name = "书房空调",
            .action = "set_temp_24",
            .action_desc = "空调设为 24°C"
          },
          {
            .device_id = "speaker",
            .device_name = "音箱",
            .action = "set_volume_30",
            .action_desc = "音量降至 30%"
          }
        },
      .description = "工作模式：灯光全亮，空调 24°C，低音量"
    },

  /* SCENE_ENTERTAINMENT - music/TV, ambient lighting */

  [SCENE_ENTERTAINMENT] =
    {
      .scene = SCENE_ENTERTAINMENT,
      .num_actions = 4,
      .actions =
        {
          {
            .device_id = "light_living",
            .device_name = "客厅灯",
            .action = "on_brightness_30",
            .action_desc = "客厅灯调至 30%"
          },
          {
            .device_id = "light_ambient",
            .device_name = "氛围灯",
            .action = "on_color_warm",
            .action_desc = "开启暖色氛围灯"
          },
          {
            .device_id = "tv_living",
            .device_name = "电视",
            .action = "on",
            .action_desc = "开启电视"
          },
          {
            .device_id = "curtain_living",
            .device_name = "客厅窗帘",
            .action = "close",
            .action_desc = "关闭窗帘"
          }
        },
      .description = "娱乐模式：氛围灯光，电视开启"
    }
};

/* Current decision */

static agent_decision_t g_current_decision;
static bool g_agent_initialized = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: apply_sensor_context
 *
 * Description:
 *   Adjust decision based on sensor data.
 *   Fine-tune actions based on actual environment conditions.
 *
 ****************************************************************************/

static void apply_sensor_context(agent_decision_t *decision,
                                 const sensor_data_t *sensor)
{
  if (!sensor || !sensor->valid)
    {
      return;
    }

  /* Adjust AC temperature based on actual temperature */

  for (int i = 0; i < decision->num_actions; i++)
    {
      if (strncmp(decision->actions[i].device_id, "ac_", 3) == 0)
        {
          /* If room is already cool, don't lower AC too much */

          if (sensor->temperature < 22.0f)
            {
              decision->actions[i].action = "set_temp_26";
              decision->actions[i].action_desc = "空调设为 26°C（室温已较低）";
            }
          else if (sensor->temperature > 28.0f)
            {
              decision->actions[i].action = "set_temp_23";
              decision->actions[i].action_desc = "空调设为 23°C（室温较高）";
            }
        }

      /* Adjust lights based on ambient light */

      if (strncmp(decision->actions[i].device_id, "light_", 6) == 0)
        {
          if (sensor->light > 500)
            {
              /* Already bright, reduce artificial light */

              decision->actions[i].action = "on_brightness_30";
              decision->actions[i].action_desc = "灯光调至 30%（自然光充足）";
            }
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ai_agent_init
 *
 * Description:
 *   Initialize AI Agent decision engine.
 *
 ****************************************************************************/

int ai_agent_init(void)
{
  memset(&g_current_decision, 0, sizeof(g_current_decision));
  g_agent_initialized = true;

  printf("[AGENT] AI Agent initialized with %d scene rules\n",
         SCENE_COUNT);

  return 0;
}

/****************************************************************************
 * Name: ai_agent_decide
 *
 * Description:
 *   Make a decision based on current scene and sensor data.
 *
 * Input:
 *   scene  - Detected scene type
 *   sensor - Current sensor data
 *
 * Returns:
 *   Decision with list of actions to execute.
 *
 ****************************************************************************/

agent_decision_t ai_agent_decide(scene_type_t scene,
                                 const sensor_data_t *sensor)
{
  agent_decision_t decision;

  if (!g_agent_initialized)
    {
      memset(&decision, 0, sizeof(decision));
      return decision;
    }

  /* Get base decision for scene */

  if (scene >= 0 && scene < SCENE_COUNT)
    {
      decision = g_scene_rules[scene];
    }
  else
    {
      decision = g_scene_rules[SCENE_UNKNOWN];
    }

  /* Apply sensor context to fine-tune */

  apply_sensor_context(&decision, sensor);

  /* Update current decision */

  g_current_decision = decision;

  printf("[AGENT] Decision for %s: %s\n",
         scene_type_to_string(scene), decision.description);

  return decision;
}

/****************************************************************************
 * Name: ai_agent_get_current
 *
 * Description:
 *   Get the last decision made by the agent.
 *
 ****************************************************************************/

agent_decision_t ai_agent_get_current(void)
{
  return g_current_decision;
}

/****************************************************************************
 * Name: ai_agent_cleanup
 *
 * Description:
 *   Cleanup AI Agent.
 *
 ****************************************************************************/

void ai_agent_cleanup(void)
{
  g_agent_initialized = false;
  printf("[AGENT] AI Agent cleaned up\n");
}
