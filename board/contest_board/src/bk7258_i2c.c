/****************************************************************************
 * board/contest_board/src/bk7258_i2c.c
 *
 * BK7258 I2C 驱动
 *
 * 用于:
 *   - 电容触控芯片通信 (I2C0)
 *   - 温湿度/光照传感器 (I2C1)
 *
 * 实现 NuttX I2C lowerhalf 接口。
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_I2C) && defined(CONFIG_BK7258_I2C)
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/kmalloc.h>
#include <nuttx/semaphore.h>
#include <arch/irq.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BK7258_NI2C               2

/* I2C 寄存器 (SDK确认: SOC_I2C0_REG_BASE=0x45850000, SOC_I2C1_REG_BASE=0x45860000) */
#define BK7258_I2C0_BASE          0x45850000
#define BK7258_I2C1_BASE          0x45860000

/* I2C 寄存器偏移 (SDK确认: 来自 i2c_reg.h) */
#define BK7258_I2C_CTRL           (4*0x4)     /* CONFIG: base + 4*0x4 */
#define BK7258_I2C_TAR            (4*0x4)     /* 目标地址 (与CTRL共用CONFIG寄存器) */
#define BK7258_I2C_DATA           (4*0x6)     /* DATA: base + 4*0x6 */
#define BK7258_I2C_STATUS         (4*0x5)     /* INT_STATUS: base + 4*0x5 */
#define BK7258_I2C_CLKDIV         (4*0x4)     /* FREQ_DIV: bit6, 10-bit width */
#define BK7258_I2C_INTEN          0x14
#define BK7258_I2C_INTSTS         (4*0x5)     /* INT_STATUS */
/* SLAVE_ADDR: bit16, 10-bit width */

#define BK7258_I2C_CTRL_EN        (1 << 0)
#define BK7258_I2C_CTRL_RESTART   (1 << 1)
#define BK7258_I2C_CTRL_ACK       (1 << 2)

#define BK7258_I2C_STS_TFE        (1 << 0)   /* TX FIFO 空 */
#define BK7258_I2C_STS_RFNE       (1 << 1)   /* RX FIFO 非空 */
#define BK7258_I2C_STS_BUSY       (1 << 2)   /* 总线忙 */
#define BK7258_I2C_STS_NACK       (1 << 3)   /* 未应答 */

/* 默认时钟 100kHz */
#define BK7258_I2C_DEFAULT_CLK    100000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_i2c_s
{
  struct i2c_master_s dev;      /* NuttX I2C 接口 */
  uint32_t base;                /* 寄存器基地址 */
  uint32_t frequency;           /* 时钟频率 */
  sem_t    lockSem;             /* 互斥锁 */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define I2C_REG(base, offset)    (*(volatile uint32_t *)((base) + (offset)))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_i2c_transfer(struct i2c_master_s *dev,
                                struct i2c_msg_s *msgs, int count);
static int  bk7258_i2c_reset(struct i2c_master_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct i2c_ops_s g_bk7258_i2c_ops =
{
  .transfer = bk7258_i2c_transfer,
  .reset    = bk7258_i2c_reset,
};

static struct bk7258_i2c_s g_bk7258_i2c[BK7258_NI2C];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_i2c_wait_tx_empty
 *
 * Description:
 *   等待 TX FIFO 空。
 *
 ****************************************************************************/

static int bk7258_i2c_wait_tx_empty(struct bk7258_i2c_s *priv)
{
  uint32_t timeout = 100000;

  while (timeout--)
    {
      if (I2C_REG(priv->base, BK7258_I2C_STATUS) & BK7258_I2C_STS_TFE)
        {
          return OK;
        }
    }

  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: bk7258_i2c_transfer_msg
 *
 * Description:
 *   执行单条 I2C 消息传输。
 *
 ****************************************************************************/

static int bk7258_i2c_transfer_msg(struct bk7258_i2c_s *priv,
                                    struct i2c_msg_s *msg)
{
  int ret;
  int i;

  /* 设置目标地址 (7-bit) */
  I2C_REG(priv->base, BK7258_I2C_TAR) = msg->addr & 0x7F;

  /* 写操作 */
  if ((msg->flags & I2C_M_READ) == 0)
    {
      for (i = 0; i < msg->length; i++)
        {
          I2C_REG(priv->base, BK7258_I2C_DATA) = msg->buffer[i];
        }

      ret = bk7258_i2c_wait_tx_empty(priv);
      if (ret < 0)
        {
          return ret;
        }
    }
  /* 读操作 */
  else
    {
      /* 发送读请求 */
      for (i = 0; i < msg->length; i++)
        {
          I2C_REG(priv->base, BK7258_I2C_DATA) = 0x100;  /* 读命令 */
        }

      ret = bk7258_i2c_wait_tx_empty(priv);
      if (ret < 0)
        {
          return ret;
        }

      /* 读取数据 */
      for (i = 0; i < msg->length; i++)
        {
          uint32_t timeout = 100000;
          while (timeout--)
            {
              if (I2C_REG(priv->base, BK7258_I2C_STATUS) & BK7258_I2C_STS_RFNE)
                {
                  break;
                }
            }
          if (timeout == 0)
            {
              return -ETIMEDOUT;
            }
          msg->buffer[i] = (uint8_t)I2C_REG(priv->base, BK7258_I2C_DATA);
        }
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_i2c_transfer
 *
 * Description:
 *   执行 I2C 传输 (支持多条消息组合)。
 *
 ****************************************************************************/

static int bk7258_i2c_transfer(struct i2c_master_s *dev,
                               struct i2c_msg_s *msgs, int count)
{
  struct bk7258_i2c_s *priv = (struct bk7258_i2c_s *)dev;
  int ret;
  int i;

  nxsem_wait(&priv->lockSem);

  for (i = 0; i < count; i++)
    {
      ret = bk7258_i2c_transfer_msg(priv, &msgs[i]);
      if (ret < 0)
        {
          _err("I2C transfer msg %d failed: %d\n", i, ret);
          nxsem_post(&priv->lockSem);
          return ret;
        }
    }

  nxsem_post(&priv->lockSem);
  return OK;
}

/****************************************************************************
 * Name: bk7258_i2c_reset
 *
 * Description:
 *   复位 I2C 控制器。
 *
 ****************************************************************************/

static int bk7258_i2c_reset(struct i2c_master_s *dev)
{
  struct bk7258_i2c_s *priv = (struct bk7258_i2c_s *)dev;

  I2C_REG(priv->base, BK7258_I2C_CTRL) = 0;
  up_mdelay(1);
  I2C_REG(priv->base, BK7258_I2C_CTRL) = BK7258_I2C_CTRL_EN;

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_i2c_initialize
 *
 * Description:
 *   初始化 I2C 控制器并注册到 NuttX。
 *
 * Input Parameters:
 *   port      - I2C 端口号 (0 或 1)
 *   frequency - 时钟频率 (Hz)
 *
 ****************************************************************************/

int bk7258_i2c_initialize(int port, uint32_t frequency)
{
  struct bk7258_i2c_s *priv;
  uint32_t clkDiv;
  char devname[16];
  int ret;

  if (port < 0 || port >= BK7258_NI2C)
    {
      return -EINVAL;
    }

  priv = &g_bk7258_i2c[port];
  priv->base = port == 0 ? BK7258_I2C0_BASE : BK7258_I2C1_BASE;
  priv->frequency = frequency ? frequency : BK7258_I2C_DEFAULT_CLK;

  nxsem_init(&priv->lockSem, 0, 1);

  /* 使能 I2C 时钟 (SDK确认: I2C0_CKEN=bit0, I2C1_CKEN=bit8 of SYS_CPU_DEVICE_CLK_ENABLE) */
  bk7258_peri_clk_enable(port == 0 ? (1 << 0) : (1 << 8));
  bk7258_peri_reset(port == 0 ? (1 << 0) : (1 << 8));

  /* 配置时钟分频: clkDiv = APB / (frequency × 2) */
  clkDiv = (BK7258_MAX_FREQ / 4) / (priv->frequency * 2) - 1;
  I2C_REG(priv->base, BK7258_I2C_CLKDIV) = clkDiv;

  /* 使能 I2C 控制器 */
  I2C_REG(priv->base, BK7258_I2C_CTRL) = BK7258_I2C_CTRL_EN |
                                          BK7258_I2C_CTRL_ACK;

  priv->dev.ops = &g_bk7258_i2c_ops;

  /* 注册到 NuttX */
  snprintf(devname, sizeof(devname), "/dev/i2c%d", port);
  ret = i2c_register(devname, &priv->dev);
  if (ret < 0)
    {
      _err("i2c_register(%s) failed: %d\n", devname, ret);
      return ret;
    }

  syslog(LOG_INFO, "I2C%d registered: %s @ %u Hz\n",
         port, devname, priv->frequency);
  return OK;
}

#endif /* CONFIG_I2C && CONFIG_BK7258_I2C */
