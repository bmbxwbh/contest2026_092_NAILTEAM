/****************************************************************************
 * board/contest_board/src/bk7258_wdt.c
 *
 * BK7258 Watchdog Driver - NuttX Lower-Half Implementation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#if defined(CONFIG_WATCHDOG) && defined(CONFIG_BK7258_WDT)

#include <nuttx/config.h>
#include <nuttx/timers/watchdog.h>
#include <nuttx/kmalloc.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Watchdog registers (SDK confirmed: SOC_WDT_REG_BASE=0x44800000,
 * offsets from wdt_reg.h) */

#define BK7258_WDT_BASE          0x44800000      /* SOC_WDT_REG_BASE */
#define BK7258_WDT_CTRL          (BK7258_WDT_BASE + 0x4*4)  /* CTRL */
#define BK7258_WDT_LOAD          (BK7258_WDT_BASE + 0x04)
#define BK7258_WDT_VALUE         (BK7258_WDT_BASE + 0x08)
#define BK7258_WDT_INTCLR        (BK7258_WDT_BASE + 0x0C)
#define BK7258_WDT_RIS           (BK7258_WDT_BASE + 0x10)
#define BK7258_WDT_MIS           (BK7258_WDT_BASE + 0x14)

/* Control register bits */
#define BK7258_WDT_CTRL_EN       (1 << 0)
#define BK7258_WDT_CTRL_INTEN    (1 << 1)
#define BK7258_WDT_CTRL_RESET    (1 << 2)

/* Watchdog clock (SDK confirmed: 26MHz / divider,
 * WDT_CKEN is bit31 of SYS_CPU_DEVICE_CLK_ENABLE)
 * AON WDT: SOC_AON_WDT_REG_BASE = 0x44000600 */

#define BK7258_WDT_CLK           26000000UL

/* Maximum timeout (seconds) */
#define BK7258_WDT_MAX_TIMEOUT   10

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* BK7258 watchdog private state.
 * The first field MUST be 'const struct watchdog_ops_s *ops' so that
 * this struct is cast-compatible with struct watchdog_lowerhalf_s.
 */

struct bk7258_wdt_s
{
  FAR const struct watchdog_ops_s *ops;  /* Must be first field */
  uint32_t timeout;                      /* Current timeout (ms) */
  bool     started;                      /* Watchdog running flag */
  FAR void *upper;                       /* Upper-half handle for unregister */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define WDT_REG(offset)         (*(volatile uint32_t *)(offset))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int bk7258_wdt_start(FAR struct watchdog_lowerhalf_s *lower);
static int bk7258_wdt_stop(FAR struct watchdog_lowerhalf_s *lower);
static int bk7258_wdt_keepalive(FAR struct watchdog_lowerhalf_s *lower);
static int bk7258_wdt_getstatus(FAR struct watchdog_lowerhalf_s *lower,
                                FAR struct watchdog_status_s *status);
static int bk7258_wdt_settimeout(FAR struct watchdog_lowerhalf_s *lower,
                                 uint32_t timeout);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct watchdog_ops_s g_bk7258_wdt_ops =
{
  .start      = bk7258_wdt_start,
  .stop       = bk7258_wdt_stop,
  .keepalive  = bk7258_wdt_keepalive,
  .getstatus  = bk7258_wdt_getstatus,
  .settimeout = bk7258_wdt_settimeout,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_wdt_start
 *
 * Description:
 *   Start the watchdog timer.
 *
 ****************************************************************************/

static int bk7258_wdt_start(FAR struct watchdog_lowerhalf_s *lower)
{
  FAR struct bk7258_wdt_s *priv = (FAR struct bk7258_wdt_s *)lower;

  if (priv == NULL)
    {
      return -EINVAL;
    }

  WDT_REG(BK7258_WDT_CTRL) = BK7258_WDT_CTRL_EN | BK7258_WDT_CTRL_RESET;
  priv->started = true;

  syslog(LOG_INFO, "BK7258 WDT: started (timeout=%u ms)\n", priv->timeout);
  return OK;
}

/****************************************************************************
 * Name: bk7258_wdt_stop
 *
 * Description:
 *   Stop the watchdog timer.
 *
 ****************************************************************************/

static int bk7258_wdt_stop(FAR struct watchdog_lowerhalf_s *lower)
{
  FAR struct bk7258_wdt_s *priv = (FAR struct bk7258_wdt_s *)lower;

  if (priv == NULL)
    {
      return -EINVAL;
    }

  WDT_REG(BK7258_WDT_CTRL) = 0;
  priv->started = false;

  syslog(LOG_INFO, "BK7258 WDT: stopped\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_wdt_keepalive
 *
 * Description:
 *   Reset the watchdog timer (pet / feed the dog).
 *
 ****************************************************************************/

static int bk7258_wdt_keepalive(FAR struct watchdog_lowerhalf_s *lower)
{
  if (lower == NULL)
    {
      return -EINVAL;
    }

  /* Write any value to INTCLR to reload the counter */
  WDT_REG(BK7258_WDT_INTCLR) = 0x01;
  return OK;
}

/****************************************************************************
 * Name: bk7258_wdt_getstatus
 *
 * Description:
 *   Get the current watchdog status.
 *
 ****************************************************************************/

static int bk7258_wdt_getstatus(FAR struct watchdog_lowerhalf_s *lower,
                                FAR struct watchdog_status_s *status)
{
  FAR struct bk7258_wdt_s *priv = (FAR struct bk7258_wdt_s *)lower;

  if (priv == NULL || status == NULL)
    {
      return -EINVAL;
    }

  status->flags = 0;
  if (priv->started)
    {
      status->flags |= WDFLAGS_ACTIVE;
    }

  status->timeout = priv->timeout;
  status->timeleft = (WDT_REG(BK7258_WDT_VALUE) * 1000) / BK7258_WDT_CLK;

  return OK;
}

/****************************************************************************
 * Name: bk7258_wdt_settimeout
 *
 * Description:
 *   Set a new timeout value for the watchdog. The timeout is in
 *   milliseconds.
 *
 ****************************************************************************/

static int bk7258_wdt_settimeout(FAR struct watchdog_lowerhalf_s *lower,
                                 uint32_t timeout)
{
  FAR struct bk7258_wdt_s *priv = (FAR struct bk7258_wdt_s *)lower;
  uint32_t ticks;

  if (priv == NULL)
    {
      return -EINVAL;
    }

  if (timeout == 0 || timeout > BK7258_WDT_MAX_TIMEOUT * 1000)
    {
      syslog(LOG_ERR,
             "BK7258 WDT: invalid timeout %u ms (max %u ms)\n",
             timeout, BK7258_WDT_MAX_TIMEOUT * 1000);
      return -EINVAL;
    }

  priv->timeout = timeout;
  ticks = (timeout * BK7258_WDT_CLK) / 1000;
  WDT_REG(BK7258_WDT_LOAD) = ticks;

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_wdt_initialize
 *
 * Description:
 *   Initialize the BK7258 watchdog and register it with the NuttX
 *   watchdog subsystem.
 *
 * Input Parameters:
 *   defaultTimeout - Default timeout in milliseconds
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int bk7258_wdt_initialize(uint32_t defaultTimeout)
{
  FAR struct bk7258_wdt_s *priv = NULL;
  FAR void *handle = NULL;
  int ret;

  /* Allocate the private state structure via kmm_zalloc
   * (supports multiple instances) */

  priv = (FAR struct bk7258_wdt_s *)
    kmm_zalloc(sizeof(struct bk7258_wdt_s));
  if (priv == NULL)
    {
      syslog(LOG_ERR, "BK7258 WDT: failed to allocate private data\n");
      return -ENOMEM;
    }

  /* Initialize the private structure */

  priv->ops     = &g_bk7258_wdt_ops;
  priv->timeout = defaultTimeout;
  priv->started = false;
  priv->upper   = NULL;

  /* Set the default timeout into hardware */

  ret = bk7258_wdt_settimeout((FAR struct watchdog_lowerhalf_s *)priv,
                              defaultTimeout);
  if (ret < 0)
    {
      syslog(LOG_ERR, "BK7258 WDT: settimeout failed: %d\n", ret);
      goto errout_with_alloc;
    }

  /* Register the watchdog device with the NuttX subsystem */

  handle = watchdog_register("/dev/watchdog0",
                             (FAR struct watchdog_lowerhalf_s *)priv);
  if (handle == NULL)
    {
      syslog(LOG_ERR, "BK7258 WDT: watchdog_register failed\n");
      ret = -EIO;
      goto errout_with_alloc;
    }

  priv->upper = handle;

  syslog(LOG_INFO, "BK7258 WDT: initialized (timeout=%u ms)\n",
         defaultTimeout);
  return OK;

errout_with_alloc:
  kmm_free(priv);
  return ret;
}

#endif /* CONFIG_WATCHDOG && CONFIG_BK7258_WDT */
