/****************************************************************************
 * board/contest_board/src/bk7258_i2s.c
 *
 * BK7258 I2S 音频驱动
 *
 * 提供 NuttX I2S lowerhalf 接口, 用于:
 *   - 多麦阵列音频采集 (场景识别输入)
 *   - 扬声器音频输出 (TTS 播报)
 *
 * 接口:
 *   - I2S0: 麦克风阵列输入 (双通道, 16kHz/16bit)
 *   - I2S0: 扬声器输出 (单通道, 16kHz/16bit)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/audio/i2s.h>
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

/* I2S 实例数量 */
#define BK7258_NI2S               1

/* 默认音频参数 */
#define BK7258_I2S_DEFAULT_SRATE  16000
#define BK7258_I2S_DEFAULT_BITS   16
#define BK7258_I2S_DEFAULT_CHANS  2

/* DMA 缓冲区 */
#define BK7258_I2S_BUF_COUNT      4
#define BK7258_I2S_BUF_SIZE       (4096)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_i2s_s
{
  struct i2s_dev_s dev;         /* I2S 设备接口 */
  uint32_t base;                /* 寄存器基地址 */
  int      irq;                 /* 中断号 */
  uint32_t srate;               /* 采样率 */
  uint8_t  bitsPerSample;       /* 位深 */
  uint8_t  channels;            /* 通道数 */

  /* RX (录音) */
  sem_t    rxSem;               /* RX 完成信号量 */
  struct i2s_callback_s rxCb;   /* RX 回调 */

  /* TX (播放) */
  sem_t    txSem;               /* TX 完成信号量 */
  struct i2s_callback_s txCb;   /* TX 回调 */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define I2S_REG(base, offset)  (*(volatile uint32_t *)((base) + (offset)))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_i2s_rxreceive(struct i2s_dev_s *dev,
                                 struct ap_buffer_s *apb,
                                 struct i2s_callback_s *cb);
static int  bk7258_i2s_txsend(struct i2s_dev_s *dev,
                              struct ap_buffer_s *apb,
                              struct i2s_callback_s *cb);
static int  bk7258_i2s_stop(struct i2s_dev_s *dev,
                            enum i2s_ch_t ch);
static int  bk7258_i2s_pause(struct i2s_dev_s *dev,
                             enum i2s_ch_t ch);
static int  bk7258_i2s_resume(struct i2s_dev_s *dev,
                              enum i2s_ch_t ch);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct i2s_ops_s g_bk7258_i2s_ops =
{
  .rxreceive = bk7258_i2s_rxreceive,
  .txsend    = bk7258_i2s_txsend,
  .stop      = bk7258_i2s_stop,
  .pause     = bk7258_i2s_pause,
  .resume    = bk7258_i2s_resume,
};

static struct bk7258_i2s_s g_bk7258_i2s[BK7258_NI2S];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_i2s_isr
 *
 * Description:
 *   I2S 中断处理函数。
 *
 ****************************************************************************/

static int bk7258_i2s_isr(int irq, void *context, void *arg)
{
  struct bk7258_i2s_s *priv = (struct bk7258_i2s_s *)arg;
  uint32_t status;

  status = I2S_REG(priv->base, BK7258_I2S_INTSTS);

  /* RX 完成 */
  if (status & 0x01)
    {
      I2S_REG(priv->base, BK7258_I2S_INTSTS) = 0x01;   /* 清除中断 */
      nxsem_post(&priv->rxSem);

      if (priv->rxCb.callback)
        {
          priv->rxCb.callback(&priv->dev, priv->rxCb.arg, OK);
        }
    }

  /* TX 完成 */
  if (status & 0x02)
    {
      I2S_REG(priv->base, BK7258_I2S_INTSTS) = 0x02;
      nxsem_post(&priv->txSem);

      if (priv->txCb.callback)
        {
          priv->txCb.callback(&priv->dev, priv->txCb.arg, OK);
        }
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_i2s_config
 *
 * Description:
 *   配置 I2S 参数: 采样率、位深、通道数。
 *
 ****************************************************************************/

static int bk7258_i2s_config(struct bk7258_i2s_s *priv,
                             uint32_t srate, uint8_t bits, uint8_t chans)
{
  /* 计算分频系数: MCLK = srate × 256 */
  uint32_t mclk_div = BK7258_MAX_FREQ / (srate * 256);

  priv->srate = srate;
  priv->bitsPerSample = bits;
  priv->channels = chans;

  /* 配置采样率 */
  I2S_REG(priv->base, BK7258_I2S_SRATE) = mclk_div;

  /* 配置格式: I2S 标准, 16bit */
  I2S_REG(priv->base, BK7258_I2S_FMT) = BK7258_I2S_FMT_I2S | (bits / 8 - 1);

  return OK;
}

/****************************************************************************
 * Name: bk7258_i2s_rxreceive
 *
 * Description:
 *   接收音频数据 (启动 DMA 接收)。
 *
 ****************************************************************************/

static int bk7258_i2s_rxreceive(struct i2s_dev_s *dev,
                                struct ap_buffer_s *apb,
                                struct i2s_callback_s *cb)
{
  struct bk7258_i2s_s *priv = (struct bk7258_i2s_s *)dev;
  int ret;

  /* 保存回调 */
  priv->rxCb.callback = cb->callback;
  priv->rxCb.arg = cb->arg;

  /* 配置 DMA 目标地址 */
  I2S_REG(priv->base, BK7258_I2S_RXDMA) = (uint32_t)apb->samp;

  /* 使能 RX */
  I2S_REG(priv->base, BK7258_I2S_CTRL) |= BK7258_I2S_CTRL_RX |
                                           BK7258_I2S_CTRL_DMA;

  return OK;
}

/****************************************************************************
 * Name: bk7258_i2s_txsend
 *
 * Description:
 *   发送音频数据 (启动 DMA 发送)。
 *
 ****************************************************************************/

static int bk7258_i2s_txsend(struct i2s_dev_s *dev,
                             struct ap_buffer_s *apb,
                             struct i2s_callback_s *cb)
{
  struct bk7258_i2s_s *priv = (struct bk7258_i2s_s *)dev;

  priv->txCb.callback = cb->callback;
  priv->txCb.arg = cb->arg;

  /* 配置 DMA 源地址 */
  I2S_REG(priv->base, BK7258_I2S_TXDMA) = (uint32_t)apb->samp;

  /* 使能 TX */
  I2S_REG(priv->base, BK7258_I2S_CTRL) |= BK7258_I2S_CTRL_TX |
                                           BK7258_I2S_CTRL_DMA;

  return OK;
}

/****************************************************************************
 * Name: bk7258_i2s_stop
 *
 * Description:
 *   停止 I2S 传输。
 *
 ****************************************************************************/

static int bk7258_i2s_stop(struct i2s_dev_s *dev, enum i2s_ch_t ch)
{
  struct bk7258_i2s_s *priv = (struct bk7258_i2s_s *)dev;
  uint32_t ctrl = I2S_REG(priv->base, BK7258_I2S_CTRL);

  if (ch == I2S_RX)
    {
      ctrl &= ~BK7258_I2S_CTRL_RX;
    }
  else if (ch == I2S_TX)
    {
      ctrl &= ~BK7258_I2S_CTRL_TX;
    }
  else
    {
      ctrl &= ~(BK7258_I2S_CTRL_RX | BK7258_I2S_CTRL_TX);
    }

  I2S_REG(priv->base, BK7258_I2S_CTRL) = ctrl;
  return OK;
}

/****************************************************************************
 * Name: bk7258_i2s_pause
 *
 * Description:
 *   暂停 I2S 传输。
 *
 ****************************************************************************/

static int bk7258_i2s_pause(struct i2s_dev_s *dev, enum i2s_ch_t ch)
{
  return bk7258_i2s_stop(dev, ch);
}

/****************************************************************************
 * Name: bk7258_i2s_resume
 *
 * Description:
 *   恢复 I2S 传输。
 *
 ****************************************************************************/

static int bk7258_i2s_resume(struct i2s_dev_s *dev, enum i2s_ch_t ch)
{
  struct bk7258_i2s_s *priv = (struct bk7258_i2s_s *)dev;
  uint32_t ctrl = I2S_REG(priv->base, BK7258_I2S_CTRL);

  if (ch == I2S_RX)
    {
      ctrl |= BK7258_I2S_CTRL_RX;
    }
  else if (ch == I2S_TX)
    {
      ctrl |= BK7258_I2S_CTRL_TX;
    }

  I2S_REG(priv->base, BK7258_I2S_CTRL) = ctrl;
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_i2s_initialize
 *
 * Description:
 *   初始化 I2S 接口并返回 I2S 设备。
 *
 ****************************************************************************/

struct i2s_dev_s *bk7258_i2s_initialize(int port)
{
  struct bk7258_i2s_s *priv;
  int ret;

  if (port < 0 || port >= BK7258_NI2S)
    {
      return NULL;
    }

  priv = &g_bk7258_i2s[port];
  priv->base = BK7258_I2S0_BASE;
  priv->irq  = BK7258_IRQ_I2S0;

  /* 初始化信号量 */
  nxsem_init(&priv->rxSem, 0, 0);
  nxsem_init(&priv->txSem, 0, 0);

  /* 使能 I2S 时钟 */
  bk7258_peri_clk_enable(BK7258_PERI_CLK_I2S0);
  bk7258_peri_reset(BK7258_PERI_CLK_I2S0);

  /* 配置默认参数 */
  bk7258_i2s_config(priv, BK7258_I2S_DEFAULT_SRATE,
                    BK7258_I2S_DEFAULT_BITS,
                    BK7258_I2S_DEFAULT_CHANS);

  /* 使能中断 */
  I2S_REG(priv->base, BK7258_I2S_INTEN) = 0x03;   /* RX + TX 中断 */
  ret = irq_attach(priv->irq, bk7258_i2s_isr, priv);
  if (ret == OK)
    {
      up_enable_irq(priv->irq);
    }

  /* 使能 I2S 控制器 */
  I2S_REG(priv->base, BK7258_I2S_CTRL) = BK7258_I2S_CTRL_ENABLE |
                                          BK7258_I2S_CTRL_MCLK;

  priv->dev.ops = &g_bk7258_i2s_ops;

  syslog(LOG_INFO, "I2S%d initialized: %uHz %ubit %uch\n",
         port, priv->srate, priv->bitsPerSample, priv->channels);

  return &priv->dev;
}

/****************************************************************************
 * Name: bk7258_audio_dsp_configure
 *
 * Description:
 *   配置音频 DSP: 降噪等级、AEC、AGC。
 *
 ****************************************************************************/

int bk7258_audio_dsp_configure(uint32_t srate, uint8_t channels)
{
  volatile uint32_t *dsp = (volatile uint32_t *)BK7258_AUDIO_DSP_BASE;

  /* 使能音频 DSP */
  dsp[0] = 0x01;                        /* DSP_CTRL: 使能 */

  /* 配置降噪等级 (中等) */
  dsp[1] = 0x02;                        /* NR_LEVEL: 中等降噪 */

  /* 使能回声消除 (AEC) */
  dsp[2] = 0x01;                        /* AEC_ENABLE: 开启 */

  /* 配置自动增益 (AGC) */
  dsp[3] = 0x02;                        /* AGC_LEVEL: 中等增益 */

  syslog(LOG_INFO, "Audio DSP configured: NR=AEC=AGC=on, %uHz\n", srate);
  return OK;
}
