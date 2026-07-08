/****************************************************************************
 * AI Scene-Aware Smart Speaker - MiHome Simulator
 *
 * Simulates MiHome device control for demo purposes.
 * Outputs control actions to serial log and LVGL UI.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "scene_aware_speaker.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MIHOME_LOG_PREFIX       "[MIHOME]"
#define MIHOME_MAX_HISTORY      16

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Action history for display */

typedef struct
{
  char device_name[32];
  char action_desc[64];
  uint32_t timestamp;
} mihome_history_entry_t;

static mihome_history_entry_t g_history[MIHOME_MAX_HISTORY];
static int g_history_idx = 0;
static int g_history_count = 0;
static bool g_initialized = false;

/* Device state cache */

typedef struct
{
  const char *device_id;
  const char *device_name;
  char current_state[32];
  bool is_on;
} mihome_device_t;

static mihome_device_t g_devices[] =
{
  {"light_living",     "客厅灯",     "off",         false},
  {"light_bedroom",    "卧室灯",     "off",         false},
  {"light_kitchen",    "厨房灯",     "off",         false},
  {"light_study",      "书房灯",     "off",         false},
  {"light_ambient",    "氛围灯",     "off",         false},
  {"ac_living",        "客厅空调",   "off",         false},
  {"ac_bedroom",       "卧室空调",   "off",         false},
  {"ac_study",         "书房空调",   "off",         false},
  {"fan_kitchen",      "油烟机",     "off",         false},
  {"hood_kitchen",     "厨房排气扇", "off",         false},
  {"curtain_living",   "客厅窗帘",   "closed",      false},
  {"curtain_bedroom",  "卧室窗帘",   "closed",      false},
  {"tv_living",        "电视",       "off",         false},
  {"speaker",          "音箱",       "on_vol_50",   true},
};

#define NUM_DEVICES (sizeof(g_devices) / sizeof(g_devices[0]))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: find_device
 *
 * Description:
 *   Find device by ID in the device cache.
 *
 ****************************************************************************/

static mihome_device_t *find_device(const char *device_id)
{
  for (int i = 0; i < NUM_DEVICES; i++)
    {
      if (strcmp(g_devices[i].device_id, device_id) == 0)
        {
          return &g_devices[i];
        }
    }
  return NULL;
}

/****************************************************************************
 * Name: update_device_state
 *
 * Description:
 *   Update device state based on action.
 *
 ****************************************************************************/

static void update_device_state(mihome_device_t *device,
                                const char *action)
{
  if (!device || !action)
    {
      return;
    }

  /* Parse action and update state */

  if (strcmp(action, "on") == 0 ||
      strncmp(action, "on_", 3) == 0)
    {
      device->is_on = true;
      snprintf(device->current_state, sizeof(device->current_state),
               "%s", action);
    }
  else if (strcmp(action, "off") == 0)
    {
      device->is_on = false;
      snprintf(device->current_state, sizeof(device->current_state),
               "off");
    }
  else if (strncmp(action, "set_", 4) == 0 ||
           strncmp(action, "open", 4) == 0 ||
           strncmp(action, "close", 5) == 0)
    {
      snprintf(device->current_state, sizeof(device->current_state),
               "%s", action);
    }
}

/****************************************************************************
 * Name: add_to_history
 *
 * Description:
 *   Add action to history buffer.
 *
 ****************************************************************************/

static void add_to_history(const char *device_name,
                           const char *action_desc)
{
  /* TODO: Get actual timestamp
   *
   * struct timespec ts;
   * clock_gettime(CLOCK_REALTIME, &ts);
   * g_history[g_history_idx].timestamp = ts.tv_sec;
   */

  g_history[g_history_idx].timestamp = 0;

  snprintf(g_history[g_history_idx].device_name,
           sizeof(g_history[g_history_idx].device_name),
           "%s", device_name);

  snprintf(g_history[g_history_idx].action_desc,
           sizeof(g_history[g_history_idx].action_desc),
           "%s", action_desc);

  g_history_idx = (g_history_idx + 1) % MIHOME_MAX_HISTORY;
  if (g_history_count < MIHOME_MAX_HISTORY)
    {
      g_history_count++;
    }
}

/****************************************************************************
 * Name: log_action
 *
 * Description:
 *   Print action to serial console in a formatted way.
 *
 ****************************************************************************/

static void log_action(const mihome_device_t *device,
                       const char *action_desc)
{
  printf("%s [%s] %s -> %s\n",
         MIHOME_LOG_PREFIX,
         device->is_on ? "ON " : "OFF",
         device->device_name,
         action_desc);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mihome_sim_init
 *
 * Description:
 *   Initialize MiHome simulator.
 *
 ****************************************************************************/

int mihome_sim_init(void)
{
  memset(g_history, 0, sizeof(g_history));
  g_history_idx = 0;
  g_history_count = 0;
  g_initialized = true;

  printf("%s Simulator initialized with %d devices\n",
         MIHOME_LOG_PREFIX, NUM_DEVICES);

  /* Print initial device states */

  printf("%s --- Device States ---\n", MIHOME_LOG_PREFIX);
  for (int i = 0; i < NUM_DEVICES; i++)
    {
      printf("%s   %-12s: %s\n",
             MIHOME_LOG_PREFIX,
             g_devices[i].device_name,
             g_devices[i].current_state);
    }
  printf("%s --------------------\n", MIHOME_LOG_PREFIX);

  return 0;
}

/****************************************************************************
 * Name: mihome_sim_execute
 *
 * Description:
 *   Execute a simulated MiHome device action.
 *
 * Input:
 *   action - Action to execute (device_id, action, description)
 *
 * Returns:
 *   0 on success, negative error code on failure.
 *
 ****************************************************************************/

int mihome_sim_execute(const mihome_action_t *action)
{
  if (!action || !g_initialized)
    {
      return -EINVAL;
    }

  /* Find device */

  mihome_device_t *device = find_device(action->device_id);
  if (!device)
    {
      printf("%s Unknown device: %s\n",
             MIHOME_LOG_PREFIX, action->device_id);
      return -ENODEV;
    }

  /* Update device state */

  update_device_state(device, action->action);

  /* Log action */

  log_action(device, action->action_desc);

  /* Add to history */

  add_to_history(action->device_name, action->action_desc);

  return 0;
}

/****************************************************************************
 * Name: mihome_sim_get_history
 *
 * Description:
 *   Get action history for display.
 *
 * Output:
 *   entries - Array to fill with history entries
 *   max_entries - Maximum entries to return
 *
 * Returns:
 *   Number of entries returned.
 *
 ****************************************************************************/

int mihome_sim_get_history(mihome_history_entry_t *entries, int max_entries)
{
  if (!entries || max_entries <= 0)
    {
      return 0;
    }

  int count = (g_history_count < max_entries) ?
              g_history_count : max_entries;

  /* Copy from oldest to newest */

  int start = (g_history_count < MIHOME_MAX_HISTORY) ?
              0 : g_history_idx;

  for (int i = 0; i < count; i++)
    {
      int idx = (start + i) % MIHOME_MAX_HISTORY;
      entries[i] = g_history[idx];
    }

  return count;
}

/****************************************************************************
 * Name: mihome_sim_get_device_states
 *
 * Description:
 *   Get current device states for display.
 *
 ****************************************************************************/

int mihome_sim_get_device_states(char *buf, int buf_size)
{
  if (!buf || buf_size <= 0)
    {
      return -EINVAL;
    }

  int pos = 0;

  pos += snprintf(buf + pos, buf_size - pos,
                  "Device States:\n");

  for (int i = 0; i < NUM_DEVICES && pos < buf_size; i++)
    {
      pos += snprintf(buf + pos, buf_size - pos,
                      "  %-12s: %s [%s]\n",
                      g_devices[i].device_name,
                      g_devices[i].current_state,
                      g_devices[i].is_on ? "ON" : "OFF");
    }

  return pos;
}

/****************************************************************************
 * Name: mihome_sim_cleanup
 *
 * Description:
 *   Cleanup MiHome simulator.
 *
 ****************************************************************************/

void mihome_sim_cleanup(void)
{
  g_initialized = false;
  printf("%s Simulator cleaned up\n", MIHOME_LOG_PREFIX);
}
