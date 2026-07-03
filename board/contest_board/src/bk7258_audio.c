/****************************************************************************
 * board/contest_board/src/bk7258_audio.c
 *
 * BK7258 音频 lowerhalf 适配层
 *
 * 连接 I2S 驱动 (bk7258_i2s.c) 与 NuttX audio 子系统,
 * 对上层提供 /dev/audio0 设备, 供应用层录音/播放。
 *
 * 职责:
 *   - 管理 audio buffer 队列
 *   - 协调 I2S DMA 传输
 *   - 处理音频格式转换 (重采样/通道转换, 如需要)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/audio/audio.h>
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

#define BK7258_AUDIO_BUFFER_COUNT   4
#define BK7258_AUDIO_BUFFER_SIZE    4096

/* 音频格式 */
#define BK7258_AUDIO_DEFAULT_SRATE  16000
#define BK7258_AUDIO_DEFAULT_CHANS  2
#define BK7258_AUDIO_DEFAULT_BPS    16

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_audio_s
{
  struct audio_lowerhalf_s dev;   /* NuttX audio 接口 */
  struct i2s_dev_s *i2s;          /* I2S 设备 */
  uint32_t srate;                 /* 采样率 */
  uint8_t  channels;              /* 通道数 */
  uint8_t  bps;                   /* 位深 */
  bool     recording;             /* 录音状态 */
  bool     playing;               /* 播放状态 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bk7258_audio_s g_bk7258_audio;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_audio_getcaps(struct audio_lowerhalf_s *dev, int type,
                                 struct audio_caps_s *caps);
static int  bk7258_audio_configure(struct audio_lowerhalf_s *dev,
                                   const struct audio_caps_s *caps);
static int  bk7258_audio_start(struct audio_lowerhalf_s *dev);
static int  bk7258_audio_stop(struct audio_lowerhalf_s *dev);
static int  bk7258_audio_pause(struct audio_lowerhalf_s *dev);
static int  bk7258_audio_resume(struct audio_lowerhalf_s *dev);
static int  bk7258_audio_enqueuebuffer(struct audio_lowerhalf_s *dev,
                                       struct ap_buffer_s *apb);
static int  bk7258_audio_ioctl(struct audio_lowerhalf_s *dev, int cmd,
                               unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct audio_ops_s g_bk7258_audio_ops =
{
  .getcaps       = bk7258_audio_getcaps,
  .configure     = bk7258_audio_configure,
  .start         = bk7258_audio_start,
  .stop          = bk7258_audio_stop,
  .pause         = bk7258_audio_pause,
  .resume        = bk7258_audio_resume,
  .enqueuebuffer = bk7258_audio_enqueuebuffer,
  .ioctl         = bk7258_audio_ioctl,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_audio_getcaps
 *
 * Description:
 *   返回音频设备能力。
 *
 ****************************************************************************/

static int bk7258_audio_getcaps(struct audio_lowerhalf_s *dev, int type,
                                struct audio_caps_s *caps)
{
  struct bk7258_audio_s *priv = (struct bk7258_audio_s *)dev;

  switch (caps->ac_type)
    {
      case AUDIO_TYPE_INPUT:
        caps->ac_channels = priv->channels;
        caps->ac_samplerate = priv->srate;
        caps->ac_format.hw = AFMT_S16_LE;
        caps->ac_controls.b[0] = priv->channels;
        break;

      case AUDIO_TYPE_OUTPUT:
        caps->ac_channels = 1;
        caps->ac_samplerate = priv->srate;
        caps->ac_format.hw = AFMT_S16_LE;
        break;

      default:
        return -EINVAL;
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_audio_configure
 *
 * Description:
 *   配置音频参数。
 *
 ****************************************************************************/

static int bk7258_audio_configure(struct audio_lowerhalf_s *dev,
                                  const struct audio_caps_s *caps)
{
  struct bk7258_audio_s *priv = (struct bk7258_audio_s *)dev;

  switch (caps->ac_type)
    {
      case AUDIO_TYPE_INPUT:
        if (caps->ac_channels > 0)
          {
            priv->channels = caps->ac_channels;
          }
        if (caps->ac_samplerate > 0)
          {
            priv->srate = caps->ac_samplerate;
          }
        break;

      case AUDIO_TYPE_OUTPUT:
        /* 输出参数配置 */
        break;
    }

  /* 重新配置 I2S */
  if (priv->i2s)
    {
      I2S_RXSAMPLERATE(priv->i2s, priv->srate);
      I2S_RXDATAWIDTH(priv->i2s, priv->bps);
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_audio_start
 *
 * Description:
 *   启动音频传输。
 *
 ****************************************************************************/

static int bk7258_audio_start(struct audio_lowerhalf_s *dev)
{
  struct bk7258_audio_s *priv = (struct bk7258_audio_s *)dev;

  /* 配置音频 DSP */
  bk7258_audio_dsp_configure(priv->srate, priv->channels);

  syslog(LOG_INFO, "Audio started: %uHz %uch %ubit\n",
         priv->srate, priv->channels, priv->bps);
  return OK;
}

/****************************************************************************
 * Name: bk7258_audio_stop
 *
 * Description:
 *   停止音频传输。
 *
 ****************************************************************************/

static int bk7258_audio_stop(struct audio_lowerhalf_s *dev)
{
  struct bk7258_audio_s *priv = (struct bk7258_audio_s *)dev;

  if (priv->i2s)
    {
      I2S_STOP(priv->i2s, I2S_RX | I2S_TX);
    }

  priv->recording = false;
  priv->playing = false;
  syslog(LOG_INFO, "Audio stopped\n");
  return OK;
}

/****************************************************************************
 * Name: bk7258_audio_pause
 *
 * Description:
 *   暂停音频传输。
 *
 ****************************************************************************/

static int bk7258_audio_pause(struct audio_lowerhalf_s *dev)
{
  struct bk7258_audio_s *priv = (struct bk7258_audio_s *)dev;
  if (priv->i2s)
    {
      I2S_PAUSE(priv->i2s, I2S_RX | I2S_TX);
    }
  return OK;
}

/****************************************************************************
 * Name: bk7258_audio_resume
 *
 * Description:
 *   恢复音频传输。
 *
 ****************************************************************************/

static int bk7258_audio_resume(struct audio_lowerhalf_s *dev)
{
  struct bk7258_audio_s *priv = (struct bk7258_audio_s *)dev;
  if (priv->i2s)
    {
      I2S_RESUME(priv->i2s, I2S_RX | I2S_TX);
    }
  return OK;
}

/****************************************************************************
 * Name: bk7258_audio_enqueuebuffer
 *
 * Description:
 *   入队音频缓冲 (供 I2S DMA 传输)。
 *
 ****************************************************************************/

static int bk7258_audio_enqueuebuffer(struct audio_lowerhalf_s *dev,
                                      struct ap_buffer_s *apb)
{
  struct bk7258_audio_s *priv = (struct bk7258_audio_s *)dev;
  struct i2s_callback_s cb;

  if (!priv->i2s)
    {
      return -ENODEV;
    }

  /* 根据 buffer 标志决定 RX/TX */
  if (apb->flags & AUDIO_APB_OUTPUT)
    {
      /* 播放: TX */
      cb.callback = NULL;  /* TODO: 完成回调 */
      cb.arg = apb;
      I2S_SEND(priv->i2s, apb, &cb);
    }
  else
    {
      /* 录音: RX */
      cb.callback = NULL;
      cb.arg = apb;
      I2S_RECEIVE(priv->i2s, apb, &cb);
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_audio_ioctl
 *
 * Description:
 *   音频设备 ioctl。
 *
 ****************************************************************************/

static int bk7258_audio_ioctl(struct audio_lowerhalf_s *dev, int cmd,
                              unsigned long arg)
{
  return -ENOTTY;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_audio_initialize
 *
 * Description:
 *   初始化音频设备并注册到 NuttX audio 子系统。
 *
 ****************************************************************************/

int bk7258_audio_initialize(void)
{
  struct bk7258_audio_s *priv = &g_bk7258_audio;
  int ret;

  /* 获取 I2S 设备 */
  priv->i2s = bk7258_i2s_initialize(0);
  if (priv->i2s == NULL)
    {
      _err("I2S init failed\n");
      return -ENODEV;
    }

  priv->srate = BK7258_AUDIO_DEFAULT_SRATE;
  priv->channels = BK7258_AUDIO_DEFAULT_CHANS;
  priv->bps = BK7258_AUDIO_DEFAULT_BPS;

  priv->dev.ops = &g_bk7258_audio_ops;

  /* 注册到 NuttX audio 子系统 */
  ret = audio_register("audio0", &priv->dev);
  if (ret < 0)
    {
      _err("audio_register failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "Audio device registered: /dev/audio0\n");
  return OK;
}
