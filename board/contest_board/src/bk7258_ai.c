/****************************************************************************
 * board/contest_board/src/bk7258_ai.c
 *
 * BK7258 硬件 AI 加速器驱动
 *
 * 用于端侧声音场景识别推理 (TFLite Micro 后端)。
 *
 * 支持特性:
 *   - int8/int16 量化推理
 *   - 卷积 (Conv2D)、深度卷积 (DepthwiseConv2D)
 *   - 全连接 (FullyConnected)
 *   - 池化 (AveragePool / MaxPool)
 *   - 元素级操作 (Add / Mul)
 *
 * 适配 TFLite Micro:
 *   - 注册自定义算子 (custom_op_resolver)
 *   - 权重加载到 AI 专用 SRAM (128KB)
 *   - 推理输入/输出通过 PSRAM 缓冲
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/kmalloc.h>
#include <arch/irq.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AI 推理超时 (ms) */
#define BK7258_AI_INFER_TIMEOUT   5000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_ai_s
{
  uint32_t base;               /* 寄存器基地址 */
  int      irq;                /* 中断号 */
  bool     initialized;        /* 初始化标志 */
  uint8_t  mode;               /* 推理模式 (int8/int16) */
  void    *weightAddr;         /* 权重地址 (AI SRAM) */
  sem_t    doneSem;            /* 推理完成信号量 */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define AI_REG(offset)        (*(volatile uint32_t *)(BK7258_AI_BASE + (offset)))

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bk7258_ai_s g_bk7258_ai;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int bk7258_ai_isr(int irq, void *context, void *arg);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_ai_isr
 *
 * Description:
 *   AI 加速器中断处理函数。
 *
 ****************************************************************************/

static int bk7258_ai_isr(int irq, void *context, void *arg)
{
  struct bk7258_ai_s *priv = (struct bk7258_ai_s *)arg;
  uint32_t status = AI_REG(BK7258_AI_STATUS);

  /* 检查错误 */
  if (status & BK7258_AI_STATUS_ERR)
    {
      syslog(LOG_ERR, "AI accelerator error: 0x%08x\n",
             AI_REG(BK7258_AI_ERR));
    }

  /* 推理完成 */
  if (status & BK7258_AI_STATUS_DONE)
    {
      nxsem_post(&priv->doneSem);
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_ai_accel_initialize
 *
 * Description:
 *   初始化 AI 加速器。
 *   - 使能时钟
 *   - 分配 AI 专用 SRAM
 *   - 注册中断
 *   - 复位加速器
 *
 ****************************************************************************/

int bk7258_ai_accel_initialize(void)
{
  struct bk7258_ai_s *priv = &g_bk7258_ai;
  int ret;

  priv->base = BK7258_AI_BASE;
  priv->irq  = BK7258_IRQ_AI;
  priv->mode = BK7258_AI_MODE_INT8;

  /* 初始化信号量 */
  nxsem_init(&priv->doneSem, 0, 0);

  /* 使能 AI 加速器时钟 */
  bk7258_peri_clk_enable(BK7258_PERI_CLK_AI);
  bk7258_peri_reset(BK7258_PERI_CLK_AI);

  /* 复位 AI 加速器 */
  AI_REG(BK7258_AI_CTRL) = BK7258_AI_CTRL_RESET;
  up_mdelay(10);
  AI_REG(BK7258_AI_CTRL) = 0;

  /* 设置权重地址 (AI 专用 SRAM) */
  priv->weightAddr = (void *)BK7258_AI_SRAM_BASE;
  AI_REG(BK7258_AI_WEIGHT_ADDR) = (uint32_t)priv->weightAddr;

  /* 设置默认推理模式 (int8) */
  AI_REG(BK7258_AI_MODE) = priv->mode;

  /* 注册中断 */
  ret = irq_attach(priv->irq, bk7258_ai_isr, priv);
  if (ret == OK)
    {
      AI_REG(BK7258_AI_INTEN) = 0x03;   /* 使能完成 + 错误中断 */
      up_enable_irq(priv->irq);
    }

  priv->initialized = true;
  syslog(LOG_INFO, "AI accelerator initialized (mode=int8, weight=%p)\n",
         priv->weightAddr);
  return OK;
}

/****************************************************************************
 * Name: bk7258_ai_load_weights
 *
 * Description:
 *   加载模型权重到 AI 专用 SRAM。
 *
 * Input Parameters:
 *   weights - 权重数据指针
 *   size    - 权重数据大小 (字节, 不超过 128KB)
 *
 ****************************************************************************/

int bk7258_ai_load_weights(const void *weights, uint32_t size)
{
  if (!g_bk7258_ai.initialized)
    {
      return -ENODEV;
    }

  if (size > BK7258_AI_SRAM_SIZE)
    {
      syslog(LOG_ERR, "Weights too large: %u > %u\n",
             size, BK7258_AI_SRAM_SIZE);
      return -ENOMEM;
    }

  /* 拷贝权重到 AI 专用 SRAM */
  memcpy(g_bk7258_ai.weightAddr, weights, size);

  syslog(LOG_INFO, "AI weights loaded: %u bytes\n", size);
  return OK;
}

/****************************************************************************
 * Name: bk7258_ai_infer
 *
 * Description:
 *   执行一次 AI 推理。
 *
 * Input Parameters:
 *   input  - 输入数据 (MFCC 特征等)
 *   output - 输出缓冲 (分类结果)
 *   size   - 输入数据大小 (字节)
 *
 * Returned Value:
 *   OK 成功, 负值失败
 *
 ****************************************************************************/

int bk7258_ai_infer(const void *input, void *output, uint32_t size)
{
  struct bk7258_ai_s *priv = &g_bk7258_ai;
  int ret;

  if (!priv->initialized)
    {
      return -ENODEV;
    }

  /* 设置输入/输出地址 */
  AI_REG(BK7258_AI_INPUT_ADDR)  = (uint32_t)input;
  AI_REG(BK7258_AI_OUTPUT_ADDR) = (uint32_t)output;

  /* 重置层索引 */
  AI_REG(BK7258_AI_LAYER) = 0;

  /* 启动推理 */
  AI_REG(BK7258_AI_CTRL) = BK7258_AI_CTRL_START;

  /* 等待完成 (带超时) */
  ret = nxsem_tickwait(&priv->doneSem, SEC2TICK(BK7258_AI_INFER_TIMEOUT / 1000));
  if (ret < 0)
    {
      syslog(LOG_ERR, "AI infer timeout\n");
      AI_REG(BK7258_AI_CTRL) = 0;   /* 停止推理 */
      return ret;
    }

  /* 检查错误状态 */
  if (AI_REG(BK7258_AI_STATUS) & BK7258_AI_STATUS_ERR)
    {
      syslog(LOG_ERR, "AI infer error: 0x%08x\n", AI_REG(BK7258_AI_ERR));
      return -EIO;
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_ai_set_mode
 *
 * Description:
 *   设置推理模式 (int8 / int16)。
 *
 ****************************************************************************/

int bk7258_ai_set_mode(uint8_t mode)
{
  if (mode != BK7258_AI_MODE_INT8 && mode != BK7258_AI_MODE_INT16)
    {
      return -EINVAL;
    }

  g_bk7258_ai.mode = mode;
  AI_REG(BK7258_AI_MODE) = mode;
  syslog(LOG_INFO, "AI mode set to %s\n",
         mode == BK7258_AI_MODE_INT8 ? "int8" : "int16");
  return OK;
}

/****************************************************************************
 * Name: bk7258_ai_get_status
 *
 * Description:
 *   获取 AI 加速器状态。
 *
 ****************************************************************************/

uint32_t bk7258_ai_get_status(void)
{
  return AI_REG(BK7258_AI_STATUS);
}
