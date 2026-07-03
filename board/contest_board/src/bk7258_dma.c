/****************************************************************************
 * board/contest_board/src/bk7258_dma.c
 *
 * BK7258 DMA 驱动
 *
 * 提供通用 DMA 传输服务, 用于:
 *   - I2S 音频数据传输 (多麦采集 + 扬声器输出)
 *   - WiFi 数据包传输
 *   - SPI LCD 帧缓冲传输 (如使用 SPI 接口 LCD)
 *
 * 特性:
 *   - 多通道 (8 通道)
 *   - 内存到内存 / 内存到外设 / 外设到内存
 *   - 传输完成中断回调
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
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

#define BK7258_NDMA_CHANNELS     8

/* DMA 传输方向 */
#define BK7258_DMA_M2M           0x00    /* 内存到内存 */
#define BK7258_DMA_M2P           0x01    /* 内存到外设 */
#define BK7258_DMA_P2M           0x02    /* 外设到内存 */
#define BK7258_DMA_P2P           0x03    /* 外设到外设 */

/* DMA 数据宽度 */
#define BK7258_DMA_WIDTH_8BIT    0x00
#define BK7258_DMA_WIDTH_16BIT   0x01
#define BK7258_DMA_WIDTH_32BIT   0x02

/* DMA 优先级 */
#define BK7258_DMA_PRIO_LOW      0x00
#define BK7258_DMA_PRIO_MED      0x01
#define BK7258_DMA_PRIO_HIGH     0x02
#define BK7258_DMA_PRIO_VHIGH    0x03

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_dma_ctrl_s
{
  uint32_t srcAddr;         /* 源地址 */
  uint32_t dstAddr;         /* 目标地址 */
  uint32_t transferCount;   /* 传输次数 */
  uint32_t ctrl;            /* 控制寄存器值 */
};

struct bk7258_dma_channel_s
{
  bool                   inUse;       /* 通道是否被占用 */
  dma_callback_t         callback;    /* 传输完成回调 */
  void                  *arg;         /* 回调参数 */
  sem_t                  doneSem;     /* 完成信号量 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bk7258_dma_channel_s g_dma_channels[BK7258_NDMA_CHANNELS];

/* DMA 控制器寄存器基址 (SDK确认: SOC_GENER_DMA_REG_BASE) */
#define BK7258_DMA_BASE           0x45020000

/* 通道寄存器偏移 (每通道 0x20 字节) */
#define BK7258_DMA_CH_OFFSET(n)   ((n) * 0x20)
#define BK7258_DMA_CH_SRC(n)      (BK7258_DMA_BASE + BK7258_DMA_CH_OFFSET(n) + 0x00)
#define BK7258_DMA_CH_DST(n)      (BK7258_DMA_BASE + BK7258_DMA_CH_OFFSET(n) + 0x04)
#define BK7258_DMA_CH_CNT(n)      (BK7258_DMA_BASE + BK7258_DMA_CH_OFFSET(n) + 0x08)
#define BK7258_DMA_CH_CTRL(n)     (BK7258_DMA_BASE + BK7258_DMA_CH_OFFSET(n) + 0x0C)
#define BK7258_DMA_CH_CFG(n)      (BK7258_DMA_BASE + BK7258_DMA_CH_OFFSET(n) + 0x10)

/* DMA 全局寄存器 */
#define BK7258_DMA_INTEN          (BK7258_DMA_BASE + 0x100)
#define BK7258_DMA_INTSTS         (BK7258_DMA_BASE + 0x104)
#define BK7258_DMA_CH_EN          (BK7258_DMA_BASE + 0x108)

#define BK7258_DMA_CTRL_ENABLE    (1 << 0)
#define BK7258_DMA_CTRL_INTEN     (1 << 1)
#define BK7258_DMA_CTRL_BURST     (1 << 2)

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define DMA_REG(addr)           (*(volatile uint32_t *)(addr))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_dma_isr
 *
 * Description:
 *   DMA 中断处理函数, 检查所有通道的完成状态。
 *
 ****************************************************************************/

static int bk7258_dma_isr(int irq, void *context, void *arg)
{
  uint32_t status = DMA_REG(BK7258_DMA_INTSTS);
  int i;

  for (i = 0; i < BK7258_NDMA_CHANNELS; i++)
    {
      if (status & (1 << i))
        {
          /* 清除中断标志 */
          DMA_REG(BK7258_DMA_INTSTS) = (1 << i);

          /* 禁用通道 */
          DMA_REG(BK7258_DMA_CH_EN) &= ~(1 << i);

          /* 释放通道 */
          g_dma_channels[i].inUse = false;

          /* 唤醒等待者 */
          nxsem_post(&g_dma_channels[i].doneSem);

          /* 调用回调 */
          if (g_dma_channels[i].callback)
            {
              g_dma_channels[i].callback(g_dma_channels[i].arg, OK);
            }
        }
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_dma_initialize
 *
 * Description:
 *   初始化 DMA 控制器。
 *
 ****************************************************************************/

int bk7258_dma_initialize(void)
{
  int i;
  int ret;

  for (i = 0; i < BK7258_NDMA_CHANNELS; i++)
    {
      g_dma_channels[i].inUse = false;
      g_dma_channels[i].callback = NULL;
      nxsem_init(&g_dma_channels[i].doneSem, 0, 0);
    }

  /* 使能 DMA 全局中断 */
  DMA_REG(BK7258_DMA_INTEN) = 0xFF;

  /* 注册中断 (SDK确认: GDMA=11, DMA0_NSEC=0) */
  ret = irq_attach(BK7258_IRQ_GDMA, bk7258_dma_isr, NULL);
  if (ret == OK)
    {
      up_enable_irq(BK7258_IRQ_GDMA);
    }

  syslog(LOG_INFO, "DMA initialized (%d channels)\n", BK7258_NDMA_CHANNELS);
  return OK;
}

/****************************************************************************
 * Name: bk7258_dma_alloc_channel
 *
 * Description:
 *   分配一个空闲的 DMA 通道。
 *
 * Returned Value:
 *   >= 0: 通道号
 *   < 0: 错误码
 *
 ****************************************************************************/

int bk7258_dma_alloc_channel(void)
{
  int i;
  for (i = 0; i < BK7258_NDMA_CHANNELS; i++)
    {
      if (!g_dma_channels[i].inUse)
        {
          g_dma_channels[i].inUse = true;
          return i;
        }
    }
  return -EBUSY;
}

/****************************************************************************
 * Name: bk7258_dma_free_channel
 *
 * Description:
 *   释放 DMA 通道。
 *
 ****************************************************************************/

int bk7258_dma_free_channel(int channel)
{
  if (channel < 0 || channel >= BK7258_NDMA_CHANNELS)
    {
      return -EINVAL;
    }

  DMA_REG(BK7258_DMA_CH_EN) &= ~(1 << channel);
  g_dma_channels[channel].inUse = false;
  g_dma_channels[channel].callback = NULL;
  return OK;
}

/****************************************************************************
 * Name: bk7258_dma_config
 *
 * Description:
 *   配置 DMA 传输参数。
 *
 * Input Parameters:
 *   channel  - 通道号
 *   srcAddr  - 源地址
 *   dstAddr  - 目标地址
 *   count    - 传输次数 (按 width 单位)
 *   width    - 数据宽度 (8/16/32 bit)
 *   direction - 传输方向 (M2M/M2P/P2M/P2P)
 *
 ****************************************************************************/

int bk7258_dma_config(int channel, uint32_t srcAddr, uint32_t dstAddr,
                      uint32_t count, uint8_t width, uint8_t direction)
{
  uint32_t ctrl;

  if (channel < 0 || channel >= BK7258_NDMA_CHANNELS)
    {
      return -EINVAL;
    }

  /* 设置源/目标地址 */
  DMA_REG(BK7258_DMA_CH_SRC(channel)) = srcAddr;
  DMA_REG(BK7258_DMA_CH_DST(channel)) = dstAddr;
  DMA_REG(BK7258_DMA_CH_CNT(channel)) = count;

  /* 配置控制寄存器 */
  ctrl = BK7258_DMA_CTRL_INTEN | (width << 4) | (direction << 8);
  DMA_REG(BK7258_DMA_CH_CTRL(channel)) = ctrl;

  return OK;
}

/****************************************************************************
 * Name: bk7258_dma_start
 *
 * Description:
 *   启动 DMA 传输。
 *
 * Input Parameters:
 *   channel  - 通道号
 *   callback - 传输完成回调 (可为 NULL, 改用阻塞等待)
 *   arg      - 回调参数
 *
 ****************************************************************************/

int bk7258_dma_start(int channel, dma_callback_t callback, void *arg)
{
  if (channel < 0 || channel >= BK7258_NDMA_CHANNELS)
    {
      return -EINVAL;
    }

  g_dma_channels[channel].callback = callback;
  g_dma_channels[channel].arg = arg;

  /* 使能通道, 启动传输 */
  DMA_REG(BK7258_DMA_CH_EN) |= (1 << channel);
  DMA_REG(BK7258_DMA_CH_CTRL(channel)) |= BK7258_DMA_CTRL_ENABLE;

  return OK;
}

/****************************************************************************
 * Name: bk7258_dma_wait
 *
 * Description:
 *   阻塞等待 DMA 传输完成 (带超时)。
 *
 ****************************************************************************/

int bk7258_dma_wait(int channel, uint32_t timeout_ms)
{
  if (channel < 0 || channel >= BK7258_NDMA_CHANNELS)
    {
      return -EINVAL;
    }

  return nxsem_tickwait(&g_dma_channels[channel].doneSem,
                        MSEC2TICK(timeout_ms));
}

/****************************************************************************
 * Name: bk7258_dma_transfer
 *
 * Description:
 *   一次性 DMA 传输 (配置 + 启动 + 等待)。
 *
 ****************************************************************************/

int bk7258_dma_transfer(uint32_t srcAddr, uint32_t dstAddr, uint32_t size,
                        uint8_t width, uint8_t direction, uint32_t timeout_ms)
{
  int channel;
  int ret;

  channel = bk7258_dma_alloc_channel();
  if (channel < 0)
    {
      return channel;
    }

  ret = bk7258_dma_config(channel, srcAddr, dstAddr, size, width, direction);
  if (ret < 0)
    {
      bk7258_dma_free_channel(channel);
      return ret;
    }

  ret = bk7258_dma_start(channel, NULL, NULL);
  if (ret < 0)
    {
      bk7258_dma_free_channel(channel);
      return ret;
    }

  ret = bk7258_dma_wait(channel, timeout_ms);
  bk7258_dma_free_channel(channel);

  return ret;
}
