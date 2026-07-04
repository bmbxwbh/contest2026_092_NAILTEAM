/****************************************************************************
 * board/contest_board/src/bk7258_lvgl.c
 *
 * BK7258 LVGL 适配层
 *
 * 连接 LCD 驱动 (bk7258_lcd.c) 与 LVGL 图形库,
 * 提供显示刷新、触控输入、定时器 tick。
 *
 * 职责:
 *   - LVGL 显示驱动 (flush_cb → LCD 帧缓冲)
 *   - LVGL 输入驱动 (touch_read → 触控坐标)
 *   - LVGL tick (1ms 定时)
 *   - 2D 加速集成 (可选)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_LVGL) && defined(CONFIG_BK7258_LCD)
#include <nuttx/lcd/lcd.h>
#include <nuttx/kmalloc.h>
#include <arch/chip/bk7258.h>

#include <lvgl/lvgl.h>
#include <lvgl/lv_display.h>
#include <lvgl/lv_indev.h>

#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BK7258_LVGL_HOR_RES       480
#define BK7258_LVGL_VER_RES       320
#define BK7258_LVGL_BUF_LINES     40      /* 分块渲染, 每次 40 行 */
#define BK7258_LVGL_BUF_SIZE      (BK7258_LVGL_HOR_RES * BK7258_LVGL_BUF_LINES * 2)

#define BK7258_LVGL_TICK_MS       1

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bk7258_lvgl_s
{
  lv_display_t *disp;           /* LVGL 显示 */
  lv_indev_t   *indev;          /* LVGL 输入设备 */
  void         *buf1;           /* 绘制缓冲1 */
  void         *buf2;           /* 绘制缓冲2 (双缓冲, 可选) */
  uint16_t     *fb;             /* LCD 帧缓冲 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bk7258_lvgl_s g_bk7258_lvgl;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void bk7258_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                                 uint8_t *px_map);
static void bk7258_lvgl_touch_read_cb(lv_indev_t *indev,
                                      lv_indev_data_t *data);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_lvgl_flush_cb
 *
 * Description:
 *   LVGL 显示刷新回调, 将绘制结果拷贝到 LCD 帧缓冲。
 *
 ****************************************************************************/

static void bk7258_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                                 uint8_t *px_map)
{
  uint16_t w = area->x2 - area->x1 + 1;
  uint16_t h = area->y2 - area->y1 + 1;
  uint16_t x, y;

  /* 将 LVGL 绘制区域拷贝到 LCD 帧缓冲 */
  for (y = 0; y < h; y++)
    {
      uint16_t *src = (uint16_t *)(px_map + y * w * 2);
      uint16_t *dst = &g_bk7258_lvgl.fb[(area->y1 + y) * BK7258_LVGL_HOR_RES +
                                         area->x1];
      memcpy(dst, src, w * 2);
    }

  /* 通知 LVGL 刷新完成 */
  lv_display_flush_ready(disp);
}

/****************************************************************************
 * Name: bk7258_lvgl_touch_read_cb
 *
 * Description:
 *   LVGL 触控读取回调。
 *
 ****************************************************************************/

static void bk7258_lvgl_touch_read_cb(lv_indev_t *indev,
                                      lv_indev_data_t *data)
{
  static uint16_t lastX = 0;
  static uint16_t lastY = 0;
  static bool pressed = false;

  /* TODO: 从触控驱动获取状态 */
  /* extern void bk7258_touch_poll(void);
   * extern bool bk7258_touch_get_point(uint16_t *x, uint16_t *y);
   *
   * if (bk7258_touch_get_point(&lastX, &lastY))
   *   {
   *     pressed = true;
   *   }
   * else
   *   {
   *     pressed = false;
   *   }
   */

  data->point.x = lastX;
  data->point.y = lastY;
  data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bk7258_lvgl_init
 *
 * Description:
 *   初始化 LVGL, 绑定 LCD 显示与触控输入。
 *
 ****************************************************************************/

int bk7258_lvgl_init(void)
{
  struct bk7258_lvgl_s *priv = &g_bk7258_lvgl;

  /* 获取 LCD 帧缓冲 */
  priv->fb = (uint16_t *)bk7258_lcd_get_framebuffer();
  if (priv->fb == NULL)
    {
      _err("LCD framebuffer not available\n");
      return -ENODEV;
    }

  /* 分配 LVGL 绘制缓冲 (PSRAM 中分配) */
  priv->buf1 = kmm_zalloc(BK7258_LVGL_BUF_SIZE);
  if (priv->buf1 == NULL)
    {
      _err("LVGL buffer1 alloc failed\n");
      return -ENOMEM;
    }

  priv->buf2 = kmm_zalloc(BK7258_LVGL_BUF_SIZE);
  if (priv->buf2 == NULL)
    {
      _err("LVGL buffer2 alloc failed (single buffer mode)\n");
    }

  /* 初始化 LVGL 核心 */
  lv_init();

  /* 创建显示 */
  priv->disp = lv_display_create(BK7258_LVGL_HOR_RES, BK7258_LVGL_VER_RES);
  if (priv->disp == NULL)
    {
      _err("lv_display_create failed\n");
      kmm_free(priv->buf1);
      if (priv->buf2)
        {
          kmm_free(priv->buf2);
        }
      return -ENOMEM;
    }

  lv_display_set_buffers(priv->disp, priv->buf1, priv->buf2,
                         BK7258_LVGL_BUF_SIZE,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(priv->disp, bk7258_lvgl_flush_cb);

  /* 创建触控输入设备 */
  priv->indev = lv_indev_create();
  if (priv->indev)
    {
      lv_indev_set_type(priv->indev, LV_INDEV_TYPE_POINTER);
      lv_indev_set_read_cb(priv->indev, bk7258_lvgl_touch_read_cb);
    }

  syslog(LOG_INFO, "LVGL initialized: %dx%d, buf=%p, fb=%p\n",
         BK7258_LVGL_HOR_RES, BK7258_LVGL_VER_RES, priv->buf1, priv->fb);
  return OK;
}

/****************************************************************************
 * Name: bk7258_lvgl_tick
 *
 * Description:
 *   LVGL tick 处理 (由 1ms 定时器或 SysTick 调用)。
 *
 ****************************************************************************/

void bk7258_lvgl_tick(void)
{
  lv_tick_inc(BK7258_LVGL_TICK_MS);
  lv_timer_handler();
}

/****************************************************************************
 * Name: bk7258_lvgl_task
 *
 * Description:
 *   LVGL 主任务 (可在独立线程中运行)。
 *
 ****************************************************************************/

int bk7258_lvgl_task(int argc, char *argv[])
{
  while (1)
    {
      bk7258_lvgl_tick();
      usleep(1000);   /* 1ms */
    }

  return OK;
}

/****************************************************************************
 * Name: bk7258_lvgl_start
 *
 * Description:
 *   启动 LVGL 主任务。
 *
 ****************************************************************************/

int bk7258_lvgl_start(void)
{
  int ret;

  ret = bk7258_lvgl_init();
  if (ret < 0)
    {
      return ret;
    }

  /* TODO: 创建 LVGL 主任务 */
  /* task_create("lvgl", CONFIG_LVGL_TASK_PRIORITY,
   *             CONFIG_LVGL_TASK_STACKSIZE,
   *             bk7258_lvgl_task, NULL);
   */

  syslog(LOG_INFO, "LVGL task started\n");
  return OK;
}

#endif /* CONFIG_LVGL && CONFIG_BK7258_LCD */
