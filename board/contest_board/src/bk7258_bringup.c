/****************************************************************************
 * board/contest_board/src/bk7258_bringup.c
 *
 * BK7258 devkit 板级外设 bringup (完整版)
 *
 * 按 NuttX 标准初始化流程拆分为四个阶段:
 *
 *   board_early_initialize()       — L0: boot/clock/uart/gpio/irq
 *                                    无阻塞、无堆、无总线操作
 *
 *   board_late_initialize()        — L1: flash/timer/i2c/spi/dma/pwm
 *                                    L2: wifi/ble
 *                                    L3: audio/lcd/touch/lvgl
 *                                    调度器就绪, 允许阻塞
 *
 *   board_app_initialize()         — L4: AI 加速器 / TFLite Micro
 *                                    L5: PM / WDT
 *                                    NSH 任务, 文件系统尚未挂载
 *
 *   board_app_finalinitialize()    — (预留) 需文件系统/KVDB 的驱动
 *
 * 每个驱动初始化:
 *   - 独立 #if defined(CONFIG_XXX) 守卫
 *   - 失败用 syslog(LOG_ERR, ...) 记录, 不中止启动
 *   - 总是返回 OK
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <arch/board/board.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <debug.h>
#include <errno.h>

/****************************************************************************
 * External Driver Initialization Functions
 ****************************************************************************/

/* L0: 基础外设 (board_early_initialize) */

extern int  bk7258_uart_initialize(int port);
extern int  bk7258_gpio_initialize(void);
extern void bk7258_irq_initialize(void);

/* L1: 存储与总线 (board_late_initialize — 需调度器) */

#if defined(CONFIG_BK7258_FLASH)
extern int  bk7258_flash_create_partitions(void);
#endif

#if defined(CONFIG_BK7258_TIMER)
extern int  bk7258_timer_initialize(void);
#endif

#if defined(CONFIG_BK7258_I2C)
extern int  bk7258_i2c_initialize(int port, uint32_t frequency);
#endif

#if defined(CONFIG_BK7258_DMA)
extern int  bk7258_dma_initialize(void);
#endif

#if defined(CONFIG_BK7258_PWM)
extern int  bk7258_pwm_initialize(void);
#endif

/* L2: 无线网络 (board_late_initialize) */

#if defined(CONFIG_BK7258_WIFI6)
extern int  bk7258_wifi_fw_load(void);
extern int  bk7258_wlan_initialize(void);
#endif

#if defined(CONFIG_BK7258_BLE54)
extern int  bk7258_ble_fw_load(void);
extern int  bk7258_ble_initialize(void);
#endif

/* L3: 多媒体 (board_late_initialize) */

#if defined(CONFIG_BK7258_AUDIO_DSP)
extern int  bk7258_audio_initialize(void);
#endif

#if defined(CONFIG_BK7258_LCD)
extern int  bk7258_lcd_initialize(void);
extern int  bk7258_touch_initialize(void);
extern void bk7258_set_backlight(int percent);
#endif

#if defined(CONFIG_LVGL)
extern int  bk7258_lvgl_start(void);
#endif

/* L4: AI 子系统 (board_app_initialize — 需文件系统加载模型) */

#if defined(CONFIG_BK7258_AI_ACCEL)
extern int  bk7258_ai_accel_initialize(void);
#endif

#if defined(CONFIG_TFLITE_MICRO)
extern int  bk7258_tflite_init(void);
#endif

/* L5: 电源管理 (board_app_initialize) */

#if defined(CONFIG_BK7258_PM)
extern int  bk7258_pm_initialize(void);
#endif

#if defined(CONFIG_BK7258_WDT)
extern int  bk7258_wdt_initialize(uint32_t timeout);
#endif

/****************************************************************************
 * Private Functions — L0: Early Init
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_bringup_l0
 *
 * Description:
 *   L0 基础外设: IRQ / UART / GPIO
 *   无阻塞、无堆、无总线操作。仅 register-level 初始化。
 *
 ****************************************************************************/

static int bk7258_bringup_l0(void)
{
  int ret;

  syslog(LOG_INFO, "=== L0: Boot/Clock/UART/GPIO ===\n");

  /* 中断控制器 — 纯寄存器配置, 无阻塞 */

  bk7258_irq_initialize();

  /* UART0 (NSH 控制台) — 仅配置波特率和引脚, 无总线操作 */

  ret = bk7258_uart_initialize(0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: UART0 init failed: %d\n", ret);
    }

#ifdef CONFIG_UART1_SERIAL_CONSOLE
  ret = bk7258_uart_initialize(1);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: UART1 init failed: %d\n", ret);
    }
#endif

  /* GPIO — 引脚复用和方向配置, 无阻塞 */

  ret = bk7258_gpio_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: GPIO init failed: %d\n", ret);
    }

  return OK;
}

/****************************************************************************
 * Private Functions — L1: Storage & Bus (needs scheduler)
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_bringup_l1
 *
 * Description:
 *   L1 存储与总线: Flash/MTD/Timer/I2C/SPI/DMA/PWM
 *   调度器就绪, 允许阻塞操作。
 *   I2C/SPI 驱动必须在此阶段或之后初始化 (避免死锁)。
 *
 ****************************************************************************/

static int bk7258_bringup_l1(void)
{
  int ret;

  syslog(LOG_INFO, "=== L1: Storage & Bus ===\n");

  /* Flash / MTD 分区 */

#if defined(CONFIG_BK7258_FLASH)
  ret = bk7258_flash_create_partitions();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Flash partitions failed: %d\n", ret);
    }
#endif

  /* 定时器 — 需调度器支持 */

#if defined(CONFIG_BK7258_TIMER)
  ret = bk7258_timer_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Timer init failed: %d\n", ret);
    }
#endif

  /* I2C0 (触控总线) — 总线操作需调度器, 不能在 early 阶段 */

#if defined(CONFIG_BK7258_I2C)
  ret = bk7258_i2c_initialize(0, 400000);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: I2C0 init failed: %d\n", ret);
    }

  /* I2C1 (传感器总线) */

  ret = bk7258_i2c_initialize(1, 100000);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: I2C1 init failed: %d\n", ret);
    }
#endif

  /* DMA — 需堆分配和中断 */

#if defined(CONFIG_BK7258_DMA)
  ret = bk7258_dma_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: DMA init failed: %d\n", ret);
    }
#endif

  /* PWM (背光/蜂鸣器) — 需定时器就绪 */

#if defined(CONFIG_BK7258_PWM)
  ret = bk7258_pwm_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: PWM init failed: %d\n", ret);
    }
#endif

  return OK;
}

/****************************************************************************
 * Private Functions — L2: Network (after L1 buses ready)
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_bringup_l2
 *
 * Description:
 *   L2 网络: WiFi6 / BLE 5.4
 *   在 L1 总线初始化完成后执行, 固件加载可能涉及 Flash/SPI 读取。
 *
 ****************************************************************************/

static int bk7258_bringup_l2(void)
{
  int ret;

  syslog(LOG_INFO, "=== L2: Network ===\n");

#if defined(CONFIG_BK7258_WIFI6)
  /* 加载 WiFi 固件 — 可能从 Flash 读取 */

  ret = bk7258_wifi_fw_load();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: WiFi FW load failed: %d\n", ret);
    }

  /* 初始化 WiFi 驱动 */

  ret = bk7258_wlan_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: WiFi init failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_BK7258_BLE54)
  /* 加载 BLE 固件 */

  ret = bk7258_ble_fw_load();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: BLE FW load failed: %d\n", ret);
    }

  /* 初始化 BLE 驱动 */

  ret = bk7258_ble_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: BLE init failed: %d\n", ret);
    }
#endif

  return OK;
}

/****************************************************************************
 * Private Functions — L3: Multimedia (after L2)
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_bringup_l3
 *
 * Description:
 *   L3 多媒体: I2S 音频 / LCD / 触控 / LVGL
 *   依赖 I2C 总线 (触控) 和 PWM (背光), 必须在 L1 之后。
 *
 ****************************************************************************/

static int bk7258_bringup_l3(void)
{
  int ret;

  syslog(LOG_INFO, "=== L3: Multimedia ===\n");

#if defined(CONFIG_BK7258_AUDIO_DSP)
  ret = bk7258_audio_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Audio init failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_BK7258_LCD)
  ret = bk7258_lcd_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: LCD init failed: %d\n", ret);
    }
  else
    {
      /* 设置默认背光 — PWM 必须已初始化 */

      bk7258_set_backlight(80);
    }

  /* 触控 — 挂在 I2C0 总线上, I2C 必须已初始化 */

  ret = bk7258_touch_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Touch init failed: %d\n", ret);
    }

  /* LVGL — 依赖 LCD 和触控 */

#  if defined(CONFIG_LVGL)
  ret = bk7258_lvgl_start();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: LVGL start failed: %d\n", ret);
    }
#  endif
#endif /* CONFIG_BK7258_LCD */

  return OK;
}

/****************************************************************************
 * Private Functions — L4: AI Subsystem (needs filesystem)
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_bringup_l4
 *
 * Description:
 *   L4 AI 子系统: AI 加速器 / TFLite Micro
 *   需要文件系统加载模型文件, 在 board_app_initialize 中调用。
 *
 ****************************************************************************/

static int bk7258_bringup_l4(void)
{
  int ret;

  syslog(LOG_INFO, "=== L4: AI Subsystem ===\n");

#if defined(CONFIG_BK7258_AI_ACCEL)
  ret = bk7258_ai_accel_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: AI accel init failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_TFLITE_MICRO)
  ret = bk7258_tflite_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: TFLite init failed: %d\n", ret);
    }
#endif

  return OK;
}

/****************************************************************************
 * Private Functions — L5: Power Management
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_bringup_l5
 *
 * Description:
 *   L5 电源管理: PM / WDT
 *   WDT 延迟到 board_app_initialize, 避免早期阶段看门狗超时
 *   导致反复重启。PM 需要各外设驱动已注册才能管理功耗状态。
 *
 ****************************************************************************/

static int bk7258_bringup_l5(void)
{
  int ret;

  syslog(LOG_INFO, "=== L5: Power Management ===\n");

#if defined(CONFIG_BK7258_WDT)
  /* 看门狗 (5秒超时) — 延后启动, 确保 bringup 不会被 WDT 咬死 */

  ret = bk7258_wdt_initialize(5000);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: WDT init failed: %d\n", ret);
    }
#endif

#if defined(CONFIG_BK7258_PM)
  /* 低功耗管理 — 需各外设驱动已注册 */

  ret = bk7258_pm_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: PM init failed: %d\n", ret);
    }
#endif

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_early_initialize
 *
 * Description:
 *   L0 基础初始化, 在 board_early_initialize() 中调用。
 *   仅 UART/GPIO/IRQ 等无需调度器的纯寄存器配置。
 *   不能阻塞、不能分配堆、不能执行总线操作。
 *
 ****************************************************************************/

int bk7258_early_initialize(void)
{
  syslog(LOG_INFO, "BK7258 early initialize...\n");

  bk7258_bringup_l0();

  syslog(LOG_INFO, "BK7258 early initialize done.\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_bringup
 *
 * Description:
 *   L1→L3 外设驱动注册, 在 board_late_initialize() 中调用。
 *   调度器就绪, 允许阻塞操作。I2C/SPI 总线初始化在此执行。
 *   每层独立容错, 单个驱动失败不影响其他。
 *
 ****************************************************************************/

int bk7258_bringup(void)
{
  syslog(LOG_INFO, "BK7258 bringup starting (L1-L3)...\n");

  /* L1: 存储与总线 — I2C/SPI 必须在此阶段初始化 */

  bk7258_bringup_l1();

  /* L2: 网络 — 依赖 L1 的 Flash/SPI 读取固件 */

  bk7258_bringup_l2();

  /* L3: 多媒体 — 依赖 L1 的 I2C (触控) 和 PWM (背光) */

  bk7258_bringup_l3();

  syslog(LOG_INFO, "BK7258 bringup complete (L1-L3).\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_app_initialize
 *
 * Description:
 *   L4→L5 应用级驱动注册, 在 board_app_initialize() 中调用。
 *   NSH 任务上下文, 文件系统尚未挂载。
 *   AI 模型加载可访问 Flash 分区; WDT 延后启动避免 early 超时。
 *
 ****************************************************************************/

int bk7258_app_initialize(void)
{
  syslog(LOG_INFO, "BK7258 app initialize (L4-L5)...\n");

  /* L4: AI 子系统 — 需文件系统/Flash 分区加载模型 */

  bk7258_bringup_l4();

  /* L5: 电源管理 — 需各驱动已注册 */

  bk7258_bringup_l5();

  syslog(LOG_INFO, "BK7258 app initialize done.\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_final_initialize
 *
 * Description:
 *   预留: 需要文件系统/KVDB 的驱动注册。
 *   在 board_app_finalinitialize() 中调用。
 *   文件系统已挂载, KVDB 已就绪。
 *
 ****************************************************************************/

#if defined(CONFIG_BOARDCTL_FINALINIT)
int bk7258_final_initialize(void)
{
  syslog(LOG_INFO, "BK7258 final initialize...\n");

  /* TODO: 需要文件系统或 KVDB 的驱动在此注册 */

  syslog(LOG_INFO, "BK7258 final initialize done.\n");
  return OK;
}
#endif
