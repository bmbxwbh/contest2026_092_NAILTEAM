/****************************************************************************
 * board/contest_board/src/bk7258_boot.c
 *
 * BK7258 devkit 板级早期启动初始化
 *
 * 此函数在 NuttX 启动早期 (head stage) 被调用, 此时:
 *   - 串口尚未初始化, 不能使用 printf
 *   - 堆尚未初始化, 不能使用 malloc
 *   - 只能做寄存器级别的硬件初始化
 *
 * 开发顺序:
 *   Phase 1: 时钟初始化 + 串口初始化 (验证: 串口有输出)
 *   Phase 2: GPIO 初始化 (LED/按键)
 *   Phase 3: 外设时钟使能 (Flash/WiFi/BT/Audio/LCD/AI)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <arch/board/board.h>
#include <arch/chip/bk7258.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* BK7258 系统控制寄存器地址 (SDK确认: SOC_SYS_REG_BASE=0x44010000) */
#define BK7258_SYS_CTRL_BASE       0x44010000
#define BK7258_CLK_CTRL_REG        (BK7258_SYS_CTRL_BASE + 0x00)
#define BK7258_CLK_DIV_REG         (BK7258_SYS_CTRL_BASE + 0x04)
#define BK7258_PERI_CLK_EN_REG     (BK7258_SYS_CTRL_BASE + 0xC*4)   /* SYS_CPU_DEVICE_CLK_ENABLE, 偏移0xC*4 */
#define BK7258_PERI_RST_REG        (BK7258_SYS_CTRL_BASE + 0x0C)

/* 外设时钟使能位 (SDK确认: SYS_CPU_DEVICE_CLK_ENABLE 偏移0xC*4)
 * Bit 0: I2C0_CKEN, Bit 1: SPI0_CKEN, Bit 2: UART0_CKEN
 * Bit 3: PWM0_CKEN, Bit 4: TIM0_CKEN, Bit 5: SADC_CKEN
 * Bit 6: IRDA_CKEN, Bit 7: EFUSE_CKEN, Bit 8: I2C1_CKEN
 * Bit 9: SPI1_CKEN, Bit 10: UART1_CKEN, Bit 11: UART2_CKEN
 * Bit 12: PWM1_CKEN, Bit 13: TIM1_CKEN, Bit 14: TIM2_CKEN
 * Bit 15: OTP_CKEN, Bit 16: I2S_CKEN, Bit 17: USB_CKEN
 * Bit 18: CAN_CKEN, Bit 19: PSRAM_CKEN, Bit 20: QSPI0_CKEN
 * Bit 21: QSPI1_CKEN, Bit 22: SDIO_CKEN, Bit 23: AUXS_CKEN
 * Bit 24: BTDM_CKEN, Bit 25: XVR_CKEN, Bit 26: MAC_CKEN
 * Bit 27: PHY_CKEN, Bit 28: JPEG_CKEN, Bit 29: DISP_CKEN
 * Bit 30: AUD_CKEN, Bit 31: WDT_CKEN */
#define BK7258_PERI_CLK_UART0      (1 << 2)       /* UART0_CKEN */
#define BK7258_PERI_CLK_UART1      (1 << 10)      /* UART1_CKEN */
#define BK7258_PERI_CLK_I2S0       (1 << 16)      /* I2S_CKEN */
#define BK7258_PERI_CLK_LCD        (1 << 29)      /* DISP_CKEN */
#define BK7258_PERI_CLK_AI         (1 << 28)      /* JPEG_CKEN (AI 共用) */
#define BK7258_PERI_CLK_WIFI       (1 << 26)      /* MAC_CKEN */
#define BK7258_PERI_CLK_BLE        (1 << 25)      /* XVR_CKEN (BLE via XVR) */
#define BK7258_PERI_CLK_FLASH      (1 << 20)      /* QSPI0_CKEN */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_clock_init
 *
 * Description:
 *   初始化系统时钟, 将主频配置到 480MHz。
 *   TODO: 需根据 BK7258 数据手册配置 PLL。
 *
 ****************************************************************************/

static void bk7258_clock_init(void)
{
  volatile uint32_t *clk_ctrl = (volatile uint32_t *)BK7258_CLK_CTRL_REG;
  volatile uint32_t *clk_div  = (volatile uint32_t *)BK7258_CLK_DIV_REG;

  /* TODO: 配置 PLL 倍频达到 480MHz
   * - 选择参考时钟源 (外部 40MHz 晶振)
   * - 配置 PLL 倍频系数 (480 / 40 = 12 倍)
   * - 切换系统时钟到 PLL 输出
   * - 等待 PLL 稳定
   *
   * 参考代码结构 (具体寄存器值待数据手册确认):
   *   *clk_ctrl = BK7258_CLK_SRC_XTAL | BK7258_PLL_ENABLE;
   *   *clk_div  = BK7258_PLL_MULT(12);
   *   while (!(*clk_ctrl & BK7258_PLL_LOCKED));
   *   *clk_ctrl |= BK7258_CLK_SRC_PLL;
   */

  (void)clk_ctrl;
  (void)clk_div;
}

/****************************************************************************
 * Name: bk7258_peri_clk_init
 *
 * Description:
 *   使能所需外设时钟, 释放外设复位。
 *
 ****************************************************************************/

static void bk7258_peri_clk_init(void)
{
  volatile uint32_t *clk_en = (volatile uint32_t *)BK7258_PERI_CLK_EN_REG;
  volatile uint32_t *rst    = (volatile uint32_t *)BK7258_PERI_RST_REG;

  uint32_t enable_mask = BK7258_PERI_CLK_UART0 |
                         BK7258_PERI_CLK_FLASH;

#ifdef CONFIG_BK7258_AUDIO_DSP
  enable_mask |= BK7258_PERI_CLK_I2S0;
#endif

#ifdef CONFIG_BK7258_LCD
  enable_mask |= BK7258_PERI_CLK_LCD;
#endif

#ifdef CONFIG_BK7258_AI_ACCEL
  enable_mask |= BK7258_PERI_CLK_AI;
#endif

#ifdef CONFIG_BK7258_WIFI6
  enable_mask |= BK7258_PERI_CLK_WIFI;
#endif

#ifdef CONFIG_BK7258_BLE54
  enable_mask |= BK7258_PERI_CLK_BLE;
#endif

  /* 使能外设时钟 */
  *clk_en |= enable_mask;

  /* 释放外设复位 (写0复位, 写1释放) */
  *rst |= enable_mask;
}

/****************************************************************************
 * Name: bk7258_uart_init
 *
 * Description:
 *   初始化 UART0 调试串口, 用于 NSH 控制台。
 *   TODO: 需根据 BK7258 UART 寄存器配置波特率/数据位/停止位。
 *
 ****************************************************************************/

static void bk7258_uart_init(void)
{
  volatile uint32_t *uart_base = (volatile uint32_t *)BK7258_UART0_BASE;

  /* TODO: 配置 UART0
   * - 设置波特率 115200 (分频系数 = 480MHz / 115200 / 16)
   * - 8 数据位, 1 停止位, 无校验
   * - 使能 FIFO
   * - 使能接收中断
   *
   * 参考代码结构:
   *   uart_base[BK7258_UART_DIV_REG] = BK7258_SYSCLK_FREQ / 115200 / 16;
   *   uart_base[BK7258_UART_LCR_REG] = BK7258_UART_LCR_8N1;
   *   uart_base[BK7258_UART_FCR_REG] = BK7258_UART_FCR_ENABLE;
   */

  (void)uart_base;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: openvela_board_initialize
 *
 * Description:
 *   板级早期初始化入口, 由 NuttX 启动流程调用。
 *   执行顺序: 时钟 → 外设时钟使能 → UART
 *
 ****************************************************************************/

void openvela_board_initialize(void)
{
  /* Phase 1: 系统时钟初始化 */
  bk7258_clock_init();

  /* Phase 2: 外设时钟使能与复位释放 */
  bk7258_peri_clk_init();

  /* Phase 3: 调试串口初始化 (NSH 控制台) */
  bk7258_uart_init();
}

/****************************************************************************
 * Name: bk7258_boardinitialize
 *
 * Description:
 *   NuttX 板级初始化标准入口, 调用 openvela_board_initialize。
 *
 ****************************************************************************/

void bk7258_boardinitialize(void)
{
  openvela_board_initialize();
}
