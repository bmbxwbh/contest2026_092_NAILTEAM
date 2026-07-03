/****************************************************************************
 * board/contest_board/src/bk7258_bringup.c
 *
 * BK7258 devkit 板级外设 bringup (完整版)
 *
 * 在 NSH 启动前被调用, 完成各外设驱动注册:
 *   L0: UART / GPIO / Timer / WDT / DMA / IRQ / PWM / I2C
 *   L1: Flash / MTD 分区 / 文件系统
 *   L2: WiFi6 / BLE 5.4
 *   L3: I2S 音频 / LCD / LVGL
 *   L4: AI 加速器 / TFLite Micro
 *   L5: 触控 / 低功耗管理
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

/* 外部驱动初始化函数声明 */
extern int bk7258_uart_initialize(int port);
extern int bk7258_gpio_initialize(void);
extern int bk7258_timer_initialize(void);
extern int bk7258_wdt_initialize(uint32_t timeout);
extern int bk7258_dma_initialize(void);
extern void bk7258_irq_initialize(void);
extern int bk7258_pwm_initialize(void);
extern int bk7258_i2c_initialize(int port, uint32_t frequency);

extern int bk7258_flash_create_partitions(void);
extern int bk7258_wifi_fw_load(void);
extern int bk7258_wlan_initialize(void);
extern int bk7258_ble_fw_load(void);
extern int bk7258_ble_initialize(void);

extern int bk7258_audio_initialize(void);
extern int bk7258_lcd_initialize(void);
extern int bk7258_touch_initialize(void);
extern int bk7258_lvgl_start(void);

extern int bk7258_ai_accel_initialize(void);
extern int bk7258_tflite_init(void);

extern int bk7258_pm_initialize(void);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_bringup_l0
 *
 * Description:
 *   L0 基础外设: UART / GPIO / Timer / WDT / DMA / IRQ / PWM / I2C
 *
 ****************************************************************************/

static int bk7258_bringup_l0(void)
{
  int ret;

  syslog(LOG_INFO, "=== L0: Base peripherals ===\n");

  /* 中断控制器 */
  bk7258_irq_initialize();

  /* UART0 (NSH 控制台) */
  ret = bk7258_uart_initialize(0);
  if (ret < 0) syslog(LOG_ERR, "UART0 init FAILED: %d\n", ret);

  /* UART1 (备用) */
#ifdef CONFIG_UART1_SERIAL_CONSOLE
  ret = bk7258_uart_initialize(1);
  if (ret < 0) syslog(LOG_ERR, "UART1 init FAILED: %d\n", ret);
#endif

  /* GPIO */
  ret = bk7258_gpio_initialize();
  if (ret < 0) syslog(LOG_ERR, "GPIO init FAILED: %d\n", ret);

  /* 定时器 */
  ret = bk7258_timer_initialize();
  if (ret < 0) syslog(LOG_ERR, "Timer init FAILED: %d\n", ret);

  /* 看门狗 (5秒超时) */
  ret = bk7258_wdt_initialize(5000);
  if (ret < 0) syslog(LOG_ERR, "WDT init FAILED: %d\n", ret);

  /* DMA */
  ret = bk7258_dma_initialize();
  if (ret < 0) syslog(LOG_ERR, "DMA init FAILED: %d\n", ret);

  /* PWM (背光/蜂鸣器) */
  ret = bk7258_pwm_initialize();
  if (ret < 0) syslog(LOG_ERR, "PWM init FAILED: %d\n", ret);

  /* I2C0 (触控) */
  ret = bk7258_i2c_initialize(0, 400000);
  if (ret < 0) syslog(LOG_ERR, "I2C0 init FAILED: %d\n", ret);

  /* I2C1 (传感器) */
  ret = bk7258_i2c_initialize(1, 100000);
  if (ret < 0) syslog(LOG_ERR, "I2C1 init FAILED: %d\n", ret);

  return OK;
}

/****************************************************************************
 * Name: bk7258_bringup_l1
 *
 * Description:
 *   L1 存储与文件系统: Flash / MTD 分区 / 文件系统挂载
 *
 ****************************************************************************/

static int bk7258_bringup_l1(void)
{
  int ret;

  syslog(LOG_INFO, "=== L1: Storage & Filesystem ===\n");

  ret = bk7258_flash_create_partitions();
  if (ret < 0) syslog(LOG_ERR, "Flash partitions FAILED: %d\n", ret);

  return OK;
}

/****************************************************************************
 * Name: bk7258_bringup_l2
 *
 * Description:
 *   L2 网络: WiFi6 / BLE 5.4
 *
 ****************************************************************************/

static int bk7258_bringup_l2(void)
{
  int ret;

  syslog(LOG_INFO, "=== L2: Network ===\n");

#ifdef CONFIG_BK7258_WIFI6
  /* 加载 WiFi 固件 */
  ret = bk7258_wifi_fw_load();
  if (ret < 0) syslog(LOG_ERR, "WiFi FW load FAILED: %d\n", ret);

  /* 初始化 WiFi */
  ret = bk7258_wlan_initialize();
  if (ret < 0) syslog(LOG_ERR, "WiFi init FAILED: %d\n", ret);
#endif

#ifdef CONFIG_BK7258_BLE54
  /* 加载 BLE 固件 */
  ret = bk7258_ble_fw_load();
  if (ret < 0) syslog(LOG_ERR, "BLE FW load FAILED: %d\n", ret);

  /* 初始化 BLE */
  ret = bk7258_ble_initialize();
  if (ret < 0) syslog(LOG_ERR, "BLE init FAILED: %d\n", ret);
#endif

  return OK;
}

/****************************************************************************
 * Name: bk7258_bringup_l3
 *
 * Description:
 *   L3 多媒体: I2S 音频 / LCD / 触控 / LVGL
 *
 ****************************************************************************/

static int bk7258_bringup_l3(void)
{
  int ret;

  syslog(LOG_INFO, "=== L3: Multimedia ===\n");

#ifdef CONFIG_BK7258_AUDIO_DSP
  ret = bk7258_audio_initialize();
  if (ret < 0) syslog(LOG_ERR, "Audio init FAILED: %d\n", ret);
#endif

#ifdef CONFIG_BK7258_LCD
  ret = bk7258_lcd_initialize();
  if (ret < 0) syslog(LOG_ERR, "LCD init FAILED: %d\n", ret);

  /* 设置默认背光 */
  bk7258_set_backlight(80);

  /* 初始化触控 */
  ret = bk7258_touch_initialize();
  if (ret < 0) syslog(LOG_ERR, "Touch init FAILED: %d\n", ret);

  /* 启动 LVGL */
#ifdef CONFIG_LVGL
  ret = bk7258_lvgl_start();
  if (ret < 0) syslog(LOG_ERR, "LVGL start FAILED: %d\n", ret);
#endif
#endif

  return OK;
}

/****************************************************************************
 * Name: bk7258_bringup_l4
 *
 * Description:
 *   L4 AI 子系统: AI 加速器 / TFLite Micro
 *
 ****************************************************************************/

static int bk7258_bringup_l4(void)
{
  int ret;

  syslog(LOG_INFO, "=== L4: AI Subsystem ===\n");

#ifdef CONFIG_BK7258_AI_ACCEL
  ret = bk7258_ai_accel_initialize();
  if (ret < 0) syslog(LOG_ERR, "AI accel init FAILED: %d\n", ret);

#ifdef CONFIG_TFLITE_MICRO
  ret = bk7258_tflite_init();
  if (ret < 0) syslog(LOG_ERR, "TFLite init FAILED: %d\n", ret);
#endif
#endif

  return OK;
}

/****************************************************************************
 * Name: bk7258_bringup_l5
 *
 * Description:
 *   L5 完整产品: 低功耗管理
 *
 ****************************************************************************/

static int bk7258_bringup_l5(void)
{
  int ret;

  syslog(LOG_INFO, "=== L5: Power Management ===\n");

  ret = bk7258_pm_initialize();
  if (ret < 0) syslog(LOG_ERR, "PM init FAILED: %d\n", ret);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_bringup
 *
 * Description:
 *   板级 bringup 总入口, 按 L0→L5 顺序注册所有外设驱动。
 *   每层独立容错, 单个失败不影响其他。
 *
 ****************************************************************************/

int bk7258_bringup(void)
{
  syslog(LOG_INFO, "BK7258 bringup starting...\n");

  bk7258_bringup_l0();
  bk7258_bringup_l1();
  bk7258_bringup_l2();
  bk7258_bringup_l3();
  bk7258_bringup_l4();
  bk7258_bringup_l5();

  syslog(LOG_INFO, "BK7258 bringup complete.\n");
  return OK;
}
