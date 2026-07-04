/****************************************************************************
 * board/contest_board/src/bk7258_touch.c
 *
 * BK7258 触控驱动
 *
 * 用于 LCD 触控交互 (场景切换、设置调节、告警确认)。
 *
 * 硬件: 通过 I2C 连接电容触控芯片 (如 GT911/FT6206)
 *       待确认 devkit 实际触控芯片型号后调整寄存器。
 *
 * 实现 NuttX touchscreen lowerhalf 接口 + input 子系统上报。
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_INPUT) && defined(CONFIG_BK7258_TOUCH)
#include <nuttx/input/touchscreen.h>
#include <nuttx/kmalloc.h>
#include <nuttx/semaphore.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 触控芯片 I2C 地址 (待确认devkit触控芯片, GT911=0x5D/0x14, FT6206=0x38)
 * BK7258 EVB 通常不自带触控芯片, 需根据实际 devkit 确认 */
#define BK7258_TOUCH_I2C_ADDR    0x38

/* I2C 端口号 (SDK确认: I2C0=0x45850000, I2C1=0x45860000) */
#define BK7258_TOUCH_I2C_PORT    0

/* 触控点最大数量 */
#define BK7258_TOUCH_MAX_POINTS  5

/* 触控芯片寄存器 (FT6206 示例, 待确认devkit触控芯片) */
#define TOUCH_REG_TD_STATUS      0x02    /* 触控点数量 */
#define TOUCH_REG_TOUCH1_XH      0x03    /* 触控点1 X 高位 */
#define TOUCH_REG_TOUCH1_XL      0x04    /* 触控点1 X 低位 */
#define TOUCH_REG_TOUCH1_YH      0x05    /* 触控点1 Y 高位 */
#define TOUCH_REG_TOUCH1_YL      0x06    /* 触控点1 Y 低位 */

/* 屏幕分辨率 (与 LCD 一致) */
#define BK7258_TOUCH_WIDTH       480
#define BK7258_TOUCH_HEIGHT      320

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_touch_s
{
  struct touchscreen_lowerhalf_s dev;  /* NuttX 触控接口 */
  uint8_t  i2cPort;                   /* I2C 端口号 */
  uint8_t  i2cAddr;                   /* I2C 设备地址 */
  uint8_t  lastTouchCount;            /* 上次触控点数 */
  bool     initialized;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bk7258_touch_s g_bk7258_touch;

/* I2C 设备句柄 (由 bk7258_i2c_initialize 返回) */
extern struct i2c_master_s *g_i2c_master[BK7258_NI2C];

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int bk7258_touch_read(struct i2c_master_s *i2c, uint8_t addr,
                             uint8_t reg, uint8_t *buf, uint8_t len);
static int bk7258_touch_write(struct i2c_master_s *i2c, uint8_t addr,
                              uint8_t reg, uint8_t value);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_touch_read
 *
 * Description:
 *   从触控芯片读取寄存器 (I2C)。
 *
 ****************************************************************************/

static int bk7258_touch_read(struct i2c_master_s *i2c, uint8_t addr,
                             uint8_t reg, uint8_t *buf, uint8_t len)
{
  struct i2c_msg_s msgs[2];
  int ret;

  /* 写寄存器地址 */
  msgs[0].addr   = addr;
  msgs[0].flags  = 0;
  msgs[0].buffer = &reg;
  msgs[0].length = 1;

  /* 读数据 */
  msgs[1].addr   = addr;
  msgs[1].flags  = I2C_M_READ;
  msgs[1].buffer = buf;
  msgs[1].length = len;

  ret = I2C_TRANSFER(i2c, msgs, 2);
  return ret;
}

/****************************************************************************
 * Name: bk7258_touch_write
 *
 * Description:
 *   写触控芯片寄存器 (I2C)。
 *
 ****************************************************************************/

static int bk7258_touch_write(struct i2c_master_s *i2c, uint8_t addr,
                              uint8_t reg, uint8_t value)
{
  struct i2c_msg_s msg;
  uint8_t buf[2];

  buf[0] = reg;
  buf[1] = value;

  msg.addr   = addr;
  msg.flags  = 0;
  msg.buffer = buf;
  msg.length = 2;

  return I2C_TRANSFER(i2c, &msg, 1);
}

/****************************************************************************
 * Name: bk7258_touch_sample
 *
 * Description:
 *   采样触控点 (定时调用)。
 *
 ****************************************************************************/

static void bk7258_touch_sample(struct bk7258_touch_s *priv)
{
  struct i2c_master_s *i2c = g_i2c_master[priv->i2cPort];
  uint8_t touchCount;
  uint8_t buf[4];
  uint16_t x, y;
  int ret;

  if (!priv->initialized || i2c == NULL)
    {
      return;
    }

  /* 读取触控点数量 */
  ret = bk7258_touch_read(i2c, priv->i2cAddr, TOUCH_REG_TD_STATUS,
                          &touchCount, 1);
  if (ret < 0)
    {
      return;
    }

  touchCount &= 0x0F;
  if (touchCount > BK7258_TOUCH_MAX_POINTS)
    {
      touchCount = BK7258_TOUCH_MAX_POINTS;
    }

  if (touchCount > 0 && touchCount != priv->lastTouchCount)
    {
      /* 读取触控点1坐标 */
      ret = bk7258_touch_read(i2c, priv->i2cAddr, TOUCH_REG_TOUCH1_XH,
                              buf, 4);
      if (ret == OK)
        {
          x = ((buf[0] & 0x0F) << 8) | buf[1];
          y = ((buf[2] & 0x0F) << 8) | buf[3];

          /* 上报触控事件 (通过 NuttX input 子系统) */
          /* TODO: 调用 touchscreen_event() 上报 */
          syslog(LOG_INFO, "Touch: (%u, %u)\n", x, y);
        }
    }

  priv->lastTouchCount = touchCount;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_touch_initialize
 *
 * Description:
 *   初始化触控芯片并注册到 NuttX。
 *
 ****************************************************************************/

int bk7258_touch_initialize(void)
{
  struct bk7258_touch_s *priv = &g_bk7258_touch;
  uint8_t chipId = 0;
  int ret;

  priv->i2cPort = BK7258_TOUCH_I2C_PORT;
  priv->i2cAddr = BK7258_TOUCH_I2C_ADDR;

  /* 读取触控芯片 ID (验证通信) */
  struct i2c_master_s *i2c = g_i2c_master[priv->i2cPort];
  if (i2c == NULL)
    {
      _err("I2C%d not initialized\n", priv->i2cPort);
      return -ENODEV;
    }

  ret = bk7258_touch_read(i2c, priv->i2cAddr, 0xA3, &chipId, 1);
  if (ret < 0)
    {
      _err("Touch chip not detected: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "Touch chip ID: 0x%02x\n", chipId);

  /* TODO: 根据芯片型号初始化配置寄存器
   * - 设置触控分辨率
   * - 设置触控灵敏度
   * - 使能中断
   */

  priv->initialized = true;
  priv->dev.dev.ops = NULL;  /* TODO: 填充 touchscreen_ops */

  /* TODO: 注册到 NuttX input 子系统 */

  syslog(LOG_INFO, "Touch initialized (%dx%d)\n",
         BK7258_TOUCH_WIDTH, BK7258_TOUCH_HEIGHT);
  return OK;
}

/****************************************************************************
 * Name: bk7258_touch_poll
 *
 * Description:
 *   轮询触控状态 (用于无中断模式)。
 *
 ****************************************************************************/

void bk7258_touch_poll(void)
{
  bk7258_touch_sample(&g_bk7258_touch);
}

#endif /* CONFIG_INPUT && CONFIG_BK7258_TOUCH */
