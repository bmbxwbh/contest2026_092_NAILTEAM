/****************************************************************************
 * board/contest_board/src/bk7258_lcd.c
 *
 * BK7258 LCD 显示驱动
 *
 * 提供 NuttX LCD lowerhalf 接口, 用于 LVGL UI 渲染。
 *
 * 规格:
 *   - 分辨率: 480 × 320 (RGB565)
 *   - 帧缓冲: PSRAM 中分配 (480 × 320 × 2 = 300KB)
 *   - 2D 加速: 矩形填充、图像 Blit
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/lcd/lcd.h>
#include <nuttx/kmalloc.h>
#include <arch/chip/bk7258.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BK7258_LCD_WIDTH_VAL      480
#define BK7258_LCD_HEIGHT_VAL     320
#define BK7258_LCD_BPP            16
#define BK7258_LCD_STRIDE         (BK7258_LCD_WIDTH_VAL * 2)

/* 帧缓冲大小: 480 × 320 × 2 = 300KB */
#define BK7258_LCD_FBSIZE         (BK7258_LCD_WIDTH_VAL * BK7258_LCD_HEIGHT_VAL * 2)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_lcd_s
{
  struct lcd_dev_s dev;        /* LCD 设备接口 */
  uint16_t *fb;                /* 帧缓冲指针 (PSRAM) */
  bool     on;                 /* 背光开关 */
};

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

#define LCD_REG(offset)       (*(volatile uint32_t *)(BK7258_LCD_BASE + (offset)))

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  bk7258_lcd_getdev(struct lcd_dev_s *dev,
                              struct lcd_devinfo_s *devinfo);
static int  bk7258_lcd_getpower(struct lcd_dev_s *dev);
static int  bk7258_lcd_setpower(struct lcd_dev_s *dev, int power);
static int  bk7258_lcd_getcontrast(struct lcd_dev_s *dev);
static int  bk7258_lcd_setcontrast(struct lcd_dev_s *dev, int contrast);
static int  bk7258_lcd_getorientation(struct lcd_dev_s *dev);
static int  bk7258_lcd_setorientation(struct lcd_dev_s *dev, int orient);
static fb_startup_t bk7258_lcd_getstartup(struct lcd_dev_s *dev,
                                          struct fb_videoinfo_s *vinfo,
                                          struct fb_planeinfo_s *pinfo);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct lcd_ops_s g_bk7258_lcd_ops =
{
  .getdev         = bk7258_lcd_getdev,
  .getpower       = bk7258_lcd_getpower,
  .setpower       = bk7258_lcd_setpower,
  .getcontrast    = bk7258_lcd_getcontrast,
  .setcontrast    = bk7258_lcd_setcontrast,
  .getorientation = bk7258_lcd_getorientation,
  .setorientation = bk7258_lcd_setorientation,
  .getstartup     = bk7258_lcd_getstartup,
};

static struct bk7258_lcd_s g_bk7258_lcd;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_lcd_getdev
 *
 * Description:
 *   返回 LCD 设备信息。
 *
 ****************************************************************************/

static int bk7258_lcd_getdev(struct lcd_dev_s *dev,
                             struct lcd_devinfo_s *devinfo)
{
  devinfo->fbmem = (void *)g_bk7258_lcd.fb;
  devinfo->fblen = BK7258_LCD_FBSIZE;
  devinfo->bpp   = BK7258_LCD_BPP;
  devinfo->stride = BK7258_LCD_STRIDE;
  devinfo->xres  = BK7258_LCD_WIDTH_VAL;
  devinfo->yres  = BK7258_LCD_HEIGHT_VAL;
  devinfo->xres_virtual = BK7258_LCD_WIDTH_VAL;
  devinfo->yres_virtual = BK7258_LCD_HEIGHT_VAL;
  devinfo->orientation = 0;
  return OK;
}

/****************************************************************************
 * Name: bk7258_lcd_getpower
 *
 * Description:
 *   获取背光状态。
 *
 ****************************************************************************/

static int bk7258_lcd_getpower(struct lcd_dev_s *dev)
{
  return g_bk7258_lcd.on ? 1 : 0;
}

/****************************************************************************
 * Name: bk7258_lcd_setpower
 *
 * Description:
 *   开关背光。
 *
 ****************************************************************************/

static int bk7258_lcd_setpower(struct lcd_dev_s *dev, int power)
{
  g_bk7258_lcd.on = (power > 0);
  LCD_REG(BK7258_LCD_CTRL) = g_bk7258_lcd.on ?
                             (LCD_REG(BK7258_LCD_CTRL) | BK7258_LCD_CTRL_ENABLE) :
                             (LCD_REG(BK7258_LCD_CTRL) & ~BK7258_LCD_CTRL_ENABLE);
  return OK;
}

/****************************************************************************
 * Name: bk7258_lcd_getcontrast
 *
 * Description:
 *   获取对比度 (LCD 不支持, 返回默认值)。
 *
 ****************************************************************************/

static int bk7258_lcd_getcontrast(struct lcd_dev_s *dev)
{
  return 0;
}

/****************************************************************************
 * Name: bk7258_lcd_setcontrast
 *
 * Description:
 *   设置对比度 (LCD 不支持)。
 *
 ****************************************************************************/

static int bk7258_lcd_setcontrast(struct lcd_dev_s *dev, int contrast)
{
  return -ENOSYS;
}

/****************************************************************************
 * Name: bk7258_lcd_getorientation
 *
 * Description:
 *   获取屏幕方向。
 *
 ****************************************************************************/

static int bk7258_lcd_getorientation(struct lcd_dev_s *dev)
{
  return 0;
}

/****************************************************************************
 * Name: bk7258_lcd_setorientation
 *
 * Description:
 *   设置屏幕方向 (暂不支持)。
 *
 ****************************************************************************/

static int bk7258_lcd_setorientation(struct lcd_dev_s *dev, int orient)
{
  return -ENOSYS;
}

/****************************************************************************
 * Name: bk7258_lcd_getstartup
 *
 * Description:
 *   返回 LCD 启动信息。
 *
 ****************************************************************************/

static fb_startup_t bk7258_lcd_getstartup(struct lcd_dev_s *dev,
                                          struct fb_videoinfo_s *vinfo,
                                          struct fb_planeinfo_s *pinfo)
{
  vinfo->fmt = FB_FMT_RGB16_565;
  vinfo->xres = BK7258_LCD_WIDTH_VAL;
  vinfo->yres = BK7258_LCD_HEIGHT_VAL;
  vinfo->nplanes = 1;

  pinfo->fbmem = (void *)g_bk7258_lcd.fb;
  pinfo->fblen = BK7258_LCD_FBSIZE;
  pinfo->stride = BK7258_LCD_STRIDE;
  pinfo->bpp = BK7258_LCD_BPP;

  return FB_RAW_MODE;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_lcd_initialize
 *
 * Description:
 *   初始化 LCD 控制器, 分配帧缓冲, 注册到 NuttX。
 *
 ****************************************************************************/

int bk7258_lcd_initialize(void)
{
  int ret;

  /* 使能 LCD 时钟 */
  bk7258_peri_clk_enable(BK7258_PERI_CLK_LCD);
  bk7258_peri_reset(BK7258_PERI_CLK_LCD);

  /* 在 PSRAM 中分配帧缓冲 */
  g_bk7258_lcd.fb = (uint16_t *)kmm_zalloc(BK7258_LCD_FBSIZE);
  if (g_bk7258_lcd.fb == NULL)
    {
      _err("LCD framebuffer alloc failed\n");
      return -ENOMEM;
    }

  /* 配置 LCD 控制器 */
  LCD_REG(BK7258_LCD_FB_ADDR) = (uint32_t)g_bk7258_lcd.fb;
  LCD_REG(BK7258_LCD_WIDTH)   = BK7258_LCD_WIDTH_VAL;
  LCD_REG(BK7258_LCD_HEIGHT)  = BK7258_LCD_HEIGHT_VAL;
  LCD_REG(BK7258_LCD_STRIDE)  = BK7258_LCD_STRIDE;
  LCD_REG(BK7258_LCD_FORMAT)  = BK7258_LCD_FMT_RGB565;

  /* 使能 2D 加速 */
#ifdef CONFIG_BK7258_2D_ACCEL
  LCD_REG(BK7258_LCD_CTRL) = BK7258_LCD_CTRL_ENABLE | BK7258_LCD_CTRL_2D;
#else
  LCD_REG(BK7258_LCD_CTRL) = BK7258_LCD_CTRL_ENABLE;
#endif

  g_bk7258_lcd.on = true;
  g_bk7258_lcd.dev.ops = &g_bk7258_lcd_ops;

  /* 注册到 NuttX */
  ret = lcd_register(&g_bk7258_lcd.dev);
  if (ret < 0)
    {
      _err("lcd_register failed: %d\n", ret);
      kmm_free(g_bk7258_lcd.fb);
      return ret;
    }

  syslog(LOG_INFO, "LCD initialized: %dx%d RGB565, fb=%p\n",
         BK7258_LCD_WIDTH_VAL, BK7258_LCD_HEIGHT_VAL, g_bk7258_lcd.fb);
  return OK;
}

/****************************************************************************
 * Name: bk7258_lcd_get_framebuffer
 *
 * Description:
 *   获取帧缓冲指针 (供 LVGL 直接绘制)。
 *
 ****************************************************************************/

void *bk7258_lcd_get_framebuffer(void)
{
  return (void *)g_bk7258_lcd.fb;
}

/****************************************************************************
 * Name: bk7258_lcd_2d_fill
 *
 * Description:
 *   2D 加速: 矩形填充。
 *
 ****************************************************************************/

int bk7258_lcd_2d_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t color)
{
#ifdef CONFIG_BK7258_2D_ACCEL
  uint32_t offset = (y * BK7258_LCD_WIDTH_VAL + x) * 2;

  LCD_REG(BK7258_2D_BLIT_DST) = (uint32_t)g_bk7258_lcd.fb + offset;
  LCD_REG(BK7258_2D_BLIT_W)   = w;
  LCD_REG(BK7258_2D_BLIT_H)   = h;
  /* TODO: 设置填充颜色寄存器 */
  LCD_REG(BK7258_2D_BLIT_CTRL) = 0x02;   /* 填充模式 */
  LCD_REG(BK7258_2D_BLIT_CTRL) |= BK7258_2D_BLIT_START;

  /* 等待完成 */
  while (LCD_REG(BK7258_2D_BLIT_CTRL) & BK7258_2D_BLIT_START)
    ;

  return OK;
#else
  /* 软件填充回退 */
  for (uint16_t j = 0; j < h; j++)
    {
      for (uint16_t i = 0; i < w; i++)
        {
          g_bk7258_lcd.fb[(y + j) * BK7258_LCD_WIDTH_VAL + (x + i)] = color;
        }
    }
  return OK;
#endif
}

/****************************************************************************
 * Name: bk7258_lcd_touch_initialize
 *
 * Description:
 *   初始化触控驱动 (TODO)。
 *
 ****************************************************************************/

int bk7258_touch_initialize(void)
{
  /* TODO: 初始化电容触控控制器
   * - 配置 I2C 接口
   * - 检测触控芯片
   * - 注册触控设备到 NuttX input 子系统
   */
  _info("Touch controller init (TODO)\n");
  return OK;
}
