# Contest 2026 team 092 - BK7258 board src
# AI场景感知智能音箱 - 板级驱动源码

nuttx_add_library(board)

# ===== L0: 基础外设 (必需) =====
target_sources(board PRIVATE
  bk7258_start.c
  bk7258_boot.c
  bk7258_bringup.c
  bk7258_appinit.c
  bk7258_reset.c
  bk7258_clock.c
  bk7258_uart.c
  bk7258_gpio.c
  bk7258_timer.c
  bk7258_irq.c
  bk7258_dma.c
  bk7258_wdt.c
  bk7258_pwm.c
  bk7258_i2c.c
)

# 启动向量表 (汇编)
target_sources(board PRIVATE
  bk7258_vectors.S
)

# ===== L1: 存储与文件系统 =====
if(CONFIG_BK7258_FLASH)
  target_sources(board PRIVATE bk7258_flash.c)
endif()

# ===== L2: 网络 =====
if(CONFIG_BK7258_WIFI6)
  target_sources(board PRIVATE
    bk7258_wlan.c
    bk7258_fw.c
  )
endif()

if(CONFIG_BK7258_BLE54)
  target_sources(board PRIVATE bk7258_ble.c)
endif()

# ===== L3: 多媒体 =====
if(CONFIG_BK7258_AUDIO_DSP)
  target_sources(board PRIVATE
    bk7258_i2s.c
    bk7258_audio.c
  )
endif()

if(CONFIG_BK7258_LCD)
  target_sources(board PRIVATE
    bk7258_lcd.c
    bk7258_touch.c
  )
endif()

if(CONFIG_LVGL)
  target_sources(board PRIVATE bk7258_lvgl.c)
endif()

# ===== L4: AI 子系统 =====
if(CONFIG_BK7258_AI_ACCEL)
  target_sources(board PRIVATE bk7258_ai.c)
endif()

if(CONFIG_TFLITE_MICRO)
  target_sources(board PRIVATE bk7258_tflite_op.cpp)
endif()

# ===== L5: 低功耗管理 =====
if(CONFIG_PM)
  target_sources(board PRIVATE bk7258_pm.c)
endif()

target_include_directories(board PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
