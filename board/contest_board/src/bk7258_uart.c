/****************************************************************************
 * board/contest_board/src/bk7258_uart.c
 *
 * BK7258 UART 驱动
 *
 * 提供 NuttX serial lowerhalf 接口, 支持:
 *   - UART0: 调试串口 (NSH 控制台, 115200 8N1)
 *   - UART1: 备用串口
 *
 * 实现的 lowerhalf 操作:
 *   - setup:    配置波特率/数据位/停止位
 *   - send:     发送一个字节 (轮询)
 *   - receive:  接收一个字节 (轮询)
 *   - txint:    发送中断使能
 *   - rxint:    接收中断使能
 *   - attach:   绑定中断回调
 *
 ****************************************************************************/

#if defined(CONFIG_SERIAL) && defined(CONFIG_BK7258_UART)

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/serial/serial.h>
#include <nuttx/kmalloc.h>
#include <nuttx/semaphore.h>
#include <arch/irq.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* UART 实例数量 */
#define BK7258_NUARTS             2

/* 默认波特率 */
#define BK7258_UART_DEFAULT_BAUD  115200

/* 接收缓冲区大小 */
#define BK7258_UART_RXBUFSIZE     256

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* UART 端口配置 */
struct bk7258_uart_s
{
  uint32_t base;           /* 寄存器基地址 */
  uint32_t baud;           /* 波特率 */
  int      irq;            /* 中断号 */
  bool     txint_enable;   /* 发送中断使能标志 */
  bool     rxint_enable;   /* 接收中断使能标志 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* UART 端口配置表 */
static struct bk7258_uart_s g_bk7258_uarts[BK7258_NUARTS] =
{
  {
    .base = BK7258_UART0_BASE,
    .baud = BK7258_UART_DEFAULT_BAUD,
    .irq  = BK7258_IRQ_UART0,
  },
  {
    .base = BK7258_UART1_BASE,
    .baud = BK7258_UART_DEFAULT_BAUD,
    .irq  = BK7258_IRQ_UART1,
  },
};

/* 接收缓冲区 (每端口独立) */
static char g_uart_rxbuf[BK7258_NUARTS][BK7258_UART_RXBUFSIZE];

/* NuttX serial lowerhalf 上下文 */
static struct uart_dev_s g_uart_devs[BK7258_NUARTS];

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_uart_setup(FAR struct uart_dev_s *dev);
static void bk7258_uart_shutdown(FAR struct uart_dev_s *dev);
static int  bk7258_uart_attach(FAR struct uart_dev_s *dev);
static void bk7258_uart_detach(FAR struct uart_dev_s *dev);
static int  bk7258_uart_ioctl(FAR struct file *filep, int cmd,
                              unsigned long arg);
static int  bk7258_uart_receive(FAR struct uart_dev_s *dev,
                                FAR unsigned int *status);
static void bk7258_uart_rxint(FAR struct uart_dev_s *dev, bool enable);
static bool bk7258_uart_rxavailable(FAR struct uart_dev_s *dev);
static void bk7258_uart_send(FAR struct uart_dev_s *dev, int ch);
static void bk7258_uart_txint(FAR struct uart_dev_s *dev, bool enable);
static bool bk7258_uart_txready(FAR struct uart_dev_s *dev);
static bool bk7258_uart_txempty(FAR struct uart_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* UART lowerhalf 操作函数表 */
static const struct uart_ops_s g_bk7258_uart_ops =
{
  .setup       = bk7258_uart_setup,
  .shutdown    = bk7258_uart_shutdown,
  .attach      = bk7258_uart_attach,
  .detach      = bk7258_uart_detach,
  .ioctl       = bk7258_uart_ioctl,
  .receive     = bk7258_uart_receive,
  .rxint       = bk7258_uart_rxint,
  .rxavailable = bk7258_uart_rxavailable,
  .send        = bk7258_uart_send,
  .txint       = bk7258_uart_txint,
  .txready     = bk7258_uart_txready,
  .txempty     = bk7258_uart_txempty,
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define UART_REG(base, offset)  (*(volatile uint32_t *)((base) + (offset)))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_uart_set_baud
 *
 * Description:
 *   配置 UART 波特率。
 *   分频系数 = 系统时钟 / (波特率 × 16)
 *
 ****************************************************************************/

static void bk7258_uart_set_baud(uint32_t base, uint32_t baud)
{
  uint32_t div = (BK7258_MAX_FREQ / (baud * 16)) - 1;
  UART_REG(base, 0x08) = div;   /* BK7258_UART_DIV */
}

/****************************************************************************
 * Name: bk7258_uart_setup
 *
 * Description:
 *   配置 UART: 波特率、数据位、停止位、校验、FIFO。
 *
 ****************************************************************************/

static int bk7258_uart_setup(FAR struct uart_dev_s *dev)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  uint32_t base = priv->base;

  /* 禁用 UART */
  UART_REG(base, 0x14) = 0;    /* 关闭中断 */

  /* 配置波特率 */
  bk7258_uart_set_baud(base, priv->baud);

  /* 配置线控制: 8 数据位, 1 停止位, 无校验 */
  UART_REG(base, 0x0C) = BK7258_UART_LCR_8N1;

  /* 使能并清空 FIFO */
  UART_REG(base, 0x10) = BK7258_UART_FCR_ENABLE | BK7258_UART_FCR_CLEAR;

  return OK;
}

/****************************************************************************
 * Name: bk7258_uart_shutdown
 *
 * Description:
 *   关闭 UART。
 *
 ****************************************************************************/

static void bk7258_uart_shutdown(FAR struct uart_dev_s *dev)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  UART_REG(priv->base, 0x14) = 0;   /* 关闭所有中断 */
}

/****************************************************************************
 * Name: bk7258_uart_attach
 *
 * Description:
 *   绑定 UART 中断处理函数。
 *
 ****************************************************************************/

static int bk7258_uart_attach(FAR struct uart_dev_s *dev)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  int ret;

  ret = irq_attach(priv->irq, uart_interrupt, dev);
  if (ret == OK)
    {
      up_enable_irq(priv->irq);
    }

  return ret;
}

/****************************************************************************
 * Name: bk7258_uart_detach
 *
 * Description:
 *   解绑 UART 中断。
 *
 ****************************************************************************/

static void bk7258_uart_detach(FAR struct uart_dev_s *dev)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  up_disable_irq(priv->irq);
  irq_detach(priv->irq);
}

/****************************************************************************
 * Name: bk7258_uart_ioctl
 *
 * Description:
 *   UART ioctl 处理 (暂未实现自定义命令)。
 *
 ****************************************************************************/

static int bk7258_uart_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  return -ENOTTY;
}

/****************************************************************************
 * Name: bk7258_uart_receive
 *
 * Description:
 *   从接收寄存器读取一个字节 (中断上下文调用)。
 *
 ****************************************************************************/

static int bk7258_uart_receive(FAR struct uart_dev_s *dev, FAR unsigned int *status)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  *status = UART_REG(priv->base, 0x04);   /* 状态寄存器 */
  return (int)UART_REG(priv->base, 0x00); /* 数据寄存器 */
}

/****************************************************************************
 * Name: bk7258_uart_rxint
 *
 * Description:
 *   使能/禁用接收中断。
 *
 ****************************************************************************/

static void bk7258_uart_rxint(FAR struct uart_dev_s *dev, bool enable)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  uint32_t ier = UART_REG(priv->base, 0x14);

  if (enable)
    {
      ier |= BK7258_UART_IER_RX;
    }
  else
    {
      ier &= ~BK7258_UART_IER_RX;
    }

  UART_REG(priv->base, 0x14) = ier;
  priv->rxint_enable = enable;
}

/****************************************************************************
 * Name: bk7258_uart_rxavailable
 *
 * Description:
 *   检查是否有数据可读。
 *
 ****************************************************************************/

static bool bk7258_uart_rxavailable(FAR struct uart_dev_s *dev)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  return (UART_REG(priv->base, 0x04) & BK7258_UART_SR_RXNE) != 0;
}

/****************************************************************************
 * Name: bk7258_uart_send
 *
 * Description:
 *   发送一个字节 (轮询模式)。
 *
 ****************************************************************************/

static void bk7258_uart_send(FAR struct uart_dev_s *dev, int ch)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;

  /* 等待发送缓冲区空 */
  while ((UART_REG(priv->base, 0x04) & BK7258_UART_SR_TXE) == 0)
    ;

  UART_REG(priv->base, 0x00) = (uint32_t)ch;
}

/****************************************************************************
 * Name: bk7258_uart_txint
 *
 * Description:
 *   使能/禁用发送中断。
 *
 ****************************************************************************/

static void bk7258_uart_txint(FAR struct uart_dev_s *dev, bool enable)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  priv->txint_enable = enable;
  /* TODO: 配置 TX 中断使能位 */
}

/****************************************************************************
 * Name: bk7258_uart_txready
 *
 * Description:
 *   检查发送是否就绪。
 *
 ****************************************************************************/

static bool bk7258_uart_txready(FAR struct uart_dev_s *dev)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  return (UART_REG(priv->base, 0x04) & BK7258_UART_SR_TXE) != 0;
}

/****************************************************************************
 * Name: bk7258_uart_txempty
 *
 * Description:
 *   检查发送是否完全完成 (FIFO 已空)。
 *
 ****************************************************************************/

static bool bk7258_uart_txempty(FAR struct uart_dev_s *dev)
{
  FAR struct bk7258_uart_s *priv = (FAR struct bk7258_uart_s *)dev->private;
  return (UART_REG(priv->base, 0x04) & BK7258_UART_SR_TXF) != 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_uart_initialize
 *
 * Description:
 *   初始化指定 UART 端口并注册到 NuttX serial 框架。
 *
 * Input Parameters:
 *   port - UART 端口号 (0 或 1)
 *
 * Returned Value:
 *   OK 成功, 负值失败
 *
 ****************************************************************************/

int bk7258_uart_initialize(int port)
{
  FAR struct uart_dev_s *dev;
  FAR struct bk7258_uart_s *priv;
  char devname[16];
  int ret;

  if (port < 0 || port >= BK7258_NUARTS)
    {
      return -EINVAL;
    }

  dev  = &g_uart_devs[port];
  priv = &g_bk7258_uarts[port];

  /* 使能 UART 时钟 */
  bk7258_peri_clk_enable(port == 0 ? BK7258_PERI_CLK_UART0 :
                                    BK7258_PERI_CLK_UART1);
  bk7258_peri_reset(port == 0 ? BK7258_PERI_CLK_UART0 :
                                BK7258_PERI_CLK_UART1);

  /* 配置 serial 设备 */
  dev->isconsole = (port == 0);
  dev->recv.size = BK7258_UART_RXBUFSIZE;
  dev->recv.buffer = g_uart_rxbuf[port];
  dev->ops = &g_bk7258_uart_ops;
  dev->private = priv;

  /* 注册到 NuttX */
  snprintf(devname, sizeof(devname), "/dev/ttyS%d", port);
  ret = uart_register(devname, dev);
  if (ret < 0)
    {
      snerr("uart_register(%s) failed: %d\n", devname, ret);
      return ret;
    }

  syslog(LOG_INFO, "UART%d registered: %s @ %u baud\n",
         port, devname, priv->baud);
  return OK;
}

/****************************************************************************
 * Name: uart_interrupt
 *
 * Description:
 *   UART 中断处理函数 (由 NuttX serial 框架调用)。
 *   实际由 nuttx/serial/serial.c 中的 uart_interrupt 调用,
 *   这里只需在 attach 时注册即可。
 *
 ****************************************************************************/

/* uart_interrupt 由 NuttX serial 框架提供, 无需在此实现 */

#endif /* CONFIG_SERIAL && CONFIG_BK7258_UART */
