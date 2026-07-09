/****************************************************************************
 * board/contest_board/include/arch/chip/bk7258.h
 *
 * BK7258 芯片寄存器定义 (完整版)
 *
 * 芯片概览:
 *   - 内核: 双核 ARM Cortex-M33 @480MHz (ARMv8-M)
 *   - 内存: 640KB 片内 SRAM + 16MB 外挂 PSRAM
 *   - AI:   硬件 AI 加速器 (int8/int16 量化推理)
 *   - 音频: 集成音频 DSP (NR/AEC/AGC) + I2S 接口
 *   - 网络: WiFi6 (802.11ax) + BLE 5.4 双模
 *   - 显示: LCD 控制器 + 2D 图形加速
 *   - 工艺: 22nm
 *
 * 寄存器基地址来源: BEKEN bk_idk SDK (reg_base.h / interrupts.h)
 * 已标注 (SDK确认) 的字段已与官方 SDK 核实。
 *
 ****************************************************************************/

#ifndef __ARCH_CHIP_BK7258_H
#define __ARCH_CHIP_BK7258_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

/****************************************************************************
 * 芯片基础信息
 ****************************************************************************/

#define BK7258_CHIP_NAME        "BK7258"
#define BK7258_CORE_NUM         2
#define BK7258_CORE_TYPE        "Cortex-M33"
#define BK7258_MAX_FREQ         480000000UL
#define BK7258_XTAL_FREQ        40000000UL

/****************************************************************************
 * 内存区域 (SDK确认)
 *
 * BK7258 片内 SRAM 由 6 个 SRAM bank 组成，总计 640KB:
 *   SRAM0: 0x28000000  64KB
 *   SRAM1: 0x28010000  64KB
 *   SRAM2: 0x28020000  128KB
 *   SRAM3: 0x28040000  128KB
 *   SRAM4: 0x28060000  128KB
 *   SRAM5: 0x28080000  128KB
 ****************************************************************************/

#define BK7258_SRAM_BASE        0x28000000
#define BK7258_SRAM_SIZE        (640 * 1024)

#define BK7258_PSRAM_BASE       0x60000000
#define BK7258_PSRAM_SIZE       (16 * 1024 * 1024)

#define BK7258_FLASH_BASE       0x02000000
#define BK7258_FLASH_SIZE       (64 * 1024 * 1024)

/* AI 加速器专用 SRAM 区域 (SRAM 中保留 128KB) */
#define BK7258_AI_SRAM_BASE     (BK7258_SRAM_BASE + 0x80000)
#define BK7258_AI_SRAM_SIZE     (128 * 1024)

/****************************************************************************
 * 外设寄存器基地址汇总 (SDK确认)
 * 来源: bk_idk/include/soc/bk7258/reg_base.h
 ****************************************************************************/

/* 系统控制 */
#define BK7258_SYS_CTRL_BASE    0x44010000      /* SOC_SYS_REG_BASE */

/* AON (Always-On) 域 */
#define BK7258_AON_PMU_BASE     0x44000000      /* SOC_AON_PMU_REG_BASE */
#define BK7258_AON_GPIO_BASE    0x44000400      /* SOC_AON_GPIO_REG_BASE */
#define BK7258_AON_RTC_BASE     0x44000200      /* SOC_AON_RTC_REG_BASE */
#define BK7258_AON_WDT_BASE     0x44000600      /* SOC_AON_WDT_REG_BASE */

/* UART */
#define BK7258_UART0_BASE       0x44820000      /* SOC_UART0_REG_BASE */
#define BK7258_UART1_BASE       0x45830000      /* SOC_UART1_REG_BASE */
#define BK7258_UART2_BASE       0x45840000      /* SOC_UART2_REG_BASE */

/* WDT */
#define BK7258_WDT_BASE         0x44800000      /* SOC_WDT_REG_BASE */

/* Timer */
#define BK7258_TIMER0_BASE      0x44810000      /* SOC_TIMER0_REG_BASE */
#define BK7258_TIMER1_BASE      0x45800000      /* SOC_TIMER1_REG_BASE */

/* Flash */
#define BK7258_FLASH_CTRL_BASE  0x44030000      /* SOC_FLASH_REG_BASE */

/* I2C */
#define BK7258_I2C0_BASE        0x45850000      /* SOC_I2C0_REG_BASE */
#define BK7258_I2C1_BASE        0x45860000      /* SOC_I2C1_REG_BASE */

/* SPI */
#define BK7258_SPI0_BASE        0x44870000      /* SOC_SPI_REG_BASE */
#define BK7258_SPI1_BASE        0x45880000      /* SOC_SPI1_REG_BASE */

/* PWM */
#define BK7258_PWM_BASE         0x458a0000      /* SOC_PWM_REG_BASE */

/* I2S */
#define BK7258_I2S0_BASE        0x47810000      /* SOC_I2S_REG_BASE */
#define BK7258_I2S1_BASE        0x47820000      /* SOC_I2S1_REG_BASE */
#define BK7258_I2S2_BASE        0x47830000      /* SOC_I2S2_REG_BASE */

/* Audio */
#define BK7258_AUDIO_DSP_BASE   0x47800000      /* SOC_AUD_REG_BASE */

/* LCD */
#define BK7258_LCD_BASE         0x48060000      /* SOC_LCD_DISP_REG_BASE */

/* DMA */
#define BK7258_DMA0_BASE        0x45020000      /* SOC_GENER_DMA_REG_BASE */
#define BK7258_DMA1_BASE        0x45030000      /* SOC_GENER_DMA1_REG_BASE */

/* DMA2D */
#define BK7258_DMA2D_BASE       0x48080000      /* SOC_DMA2D_REG_BASE */

/* PSRAM 控制器 */
#define BK7258_PSRAM_CTRL_BASE  0x46080000      /* SOC_PSRAM_REG_BASE */

/* QSPI */
#define BK7258_QSPI0_BASE       0x46040000      /* SOC_QSPI0_REG_BASE */
#define BK7258_QSPI1_BASE       0x46060000      /* SOC_QSPI1_REG_BASE */

/* WiFi / BLE XVR */
#define BK7258_XVR_BASE         0x4A800000      /* SOC_XVR_REG_BASE */

/* EFUSE */
#define BK7258_EFUSE_BASE       0x44880000      /* SOC_EFUSE_REG_BASE */

/* JPEG */
#define BK7258_JPEG_BASE        0x48000000      /* SOC_JPEG_REG_BASE */

/* H264 */
#define BK7258_H264_BASE        0x480A0000      /* SOC_H264_REG_BASE */

/* TRNG */
#define BK7258_TRNG_BASE        0x458C0000      /* SOC_TRNG_REG_BASE */

/****************************************************************************
 * 系统控制寄存器 (SDK确认)
 * 基地址: 0x44010000 (SOC_SYS_REG_BASE)
 ****************************************************************************/

#define BK7258_CLK_CTRL         (BK7258_SYS_CTRL_BASE + 0x00)
#define BK7258_CLK_DIV          (BK7258_SYS_CTRL_BASE + 0x04)
#define BK7258_PERI_CLK_EN      (BK7258_SYS_CTRL_BASE + 0xC*4)   /* SYS_CPU_DEVICE_CLK_ENABLE (SDK确认) */
#define BK7258_PERI_RST         (BK7258_SYS_CTRL_BASE + 0x0C)
#define BK7258_SW_RESET         (BK7258_SYS_CTRL_BASE + 0x10)
#define BK7258_CLK_GATING       (BK7258_SYS_CTRL_BASE + 0x14)

/* 时钟源选择 */
#define BK7258_CLK_SRC_XTAL     (0 << 0)
#define BK7258_CLK_SRC_PLL      (1 << 0)
#define BK7258_CLK_SRC_MASK     (1 << 0)

/* PLL 控制 */
#define BK7258_PLL_ENABLE       (1 << 1)
#define BK7258_PLL_LOCKED       (1 << 2)
#define BK7258_PLL_BYPASS       (1 << 3)
#define BK7258_PLL_MULT_SHIFT   8
#define BK7258_PLL_MULT(n)      ((n) << BK7258_PLL_MULT_SHIFT)

/* 外设时钟使能位 (SDK确认: SYS_CPU_DEVICE_CLK_ENABLE)
 * Bit 0: I2C0_CKEN, Bit 1: SPI0_CKEN, Bit 2: UART0_CKEN
 * Bit 3: PWM0_CKEN, Bit 4: TIM0_CKEN, Bit 5: SADC_CKEN
 * Bit 6: IRDA_CKEN, Bit 7: EFUSE_CKEN, Bit 8: I2C1_CKEN
 * Bit 9: SPI1_CKEN, Bit 10: UART1_CKEN, Bit 11: UART2_CKEN
 * Bit 12: PWM1_CKEN, Bit 13: TIM1_CKEN, Bit 14: TIM2_CKEN
 * Bit 15: OTP_CKEN, Bit 16: I2S_CKEN, Bit 17: USB_CKEN
 * Bit 18: CAN_CKEN, Bit 19: PSRAM_CKEN, Bit 20: QSPI0_CKEN
 * Bit 21: QSPI1_CKEN, Bit 22: SDIO_CKEN, Bit 23: AUXS_CKEN
 * Bit 24: BTDM_CKEN, Bit 25: XVR_CKEN, Bit 26: MAC_CKEN
 * Bit 27: PHY_CKEN, Bit 28: JPEG_CKEN, Bit 29: DISP_CKEN
 * Bit 30: AUD_CKEN, Bit 31: WDT_CKEN */
#define BK7258_PERI_CLK_I2C0    (1 << 0)       /* I2C0_CKEN */
#define BK7258_PERI_CLK_SPI0    (1 << 1)       /* SPI0_CKEN */
#define BK7258_PERI_CLK_UART0   (1 << 2)       /* UART0_CKEN */
#define BK7258_PERI_CLK_PWM0    (1 << 3)       /* PWM0_CKEN */
#define BK7258_PERI_CLK_TIMER0  (1 << 4)       /* TIM0_CKEN */
#define BK7258_PERI_CLK_SADC    (1 << 5)       /* SADC_CKEN */
#define BK7258_PERI_CLK_I2C1    (1 << 8)       /* I2C1_CKEN */
#define BK7258_PERI_CLK_SPI1    (1 << 9)       /* SPI1_CKEN */
#define BK7258_PERI_CLK_UART1   (1 << 10)      /* UART1_CKEN */
#define BK7258_PERI_CLK_UART2   (1 << 11)      /* UART2_CKEN */
#define BK7258_PERI_CLK_PWM1    (1 << 12)      /* PWM1_CKEN */
#define BK7258_PERI_CLK_I2S0    (1 << 16)      /* I2S_CKEN */
#define BK7258_PERI_CLK_QSPI0   (1 << 20)      /* QSPI0_CKEN */
#define BK7258_PERI_CLK_XVR     (1 << 25)      /* XVR_CKEN (BLE) */
#define BK7258_PERI_CLK_MAC     (1 << 26)      /* MAC_CKEN (WiFi) */
#define BK7258_PERI_CLK_JPEG    (1 << 28)      /* JPEG_CKEN */
#define BK7258_PERI_CLK_DISP    (1 << 29)      /* DISP_CKEN (LCD) */
#define BK7258_PERI_CLK_WDT     (1 << 31)      /* WDT_CKEN */

/* 兼容别名 (旧代码引用) */
#define BK7258_PERI_CLK_LCD     BK7258_PERI_CLK_DISP    /* LCD = DISP_CKEN */
#define BK7258_PERI_CLK_AI      BK7258_PERI_CLK_JPEG    /* AI = JPEG_CKEN */
#define BK7258_PERI_CLK_WIFI    BK7258_PERI_CLK_MAC     /* WiFi = MAC_CKEN */
#define BK7258_PERI_CLK_BLE     BK7258_PERI_CLK_XVR     /* BLE = XVR_CKEN */
#define BK7258_PERI_CLK_FLASH   BK7258_PERI_CLK_QSPI0   /* Flash = QSPI0_CKEN */
#define BK7258_PERI_CLK_GPIO    (1 << 30)               /* GPIO (第二CLK寄存器) */

/* 软复位 */
#define BK7258_SW_RESET_KEY     0x5A5A5A5A

/****************************************************************************
 * UART 寄存器 (SDK确认)
 * 基地址: UART0=0x44820000, UART1=0x45830000, UART2=0x45840000
 ****************************************************************************/

#define BK7258_UART_DR(n)       ((n) + 0x00)   /* 数据寄存器 */
#define BK7258_UART_SR(n)       ((n) + 0x04)   /* 状态寄存器 */
#define BK7258_UART_DIV(n)      ((n) + 0x08)   /* 波特率分频 */
#define BK7258_UART_LCR(n)      ((n) + 0x0C)   /* 线控制 */
#define BK7258_UART_FCR(n)      ((n) + 0x10)   /* FIFO 控制 */
#define BK7258_UART_IER(n)      ((n) + 0x14)   /* 中断使能 */

#define BK7258_UART_LCR_8N1     0x03           /* 8数据位 1停止位 无校验 */
#define BK7258_UART_FCR_ENABLE  0x01           /* 使能 FIFO */
#define BK7258_UART_FCR_CLEAR   0x06           /* 清空 RX/TX FIFO */

#define BK7258_UART_SR_TXE      (1 << 5)       /* 发送缓冲区空 */
#define BK7258_UART_SR_TXF      (1 << 6)       /* 发送完成 */
#define BK7258_UART_SR_RXNE     (1 << 4)       /* 接收缓冲区非空 */

#define BK7258_UART_IER_RX      (1 << 0)       /* 接收中断使能 */

/****************************************************************************
 * GPIO 寄存器 (SDK确认)
 * 基地址: 0x44000400 (SOC_AON_GPIO_REG_BASE)
 ****************************************************************************/

#define BK7258_GPIO_BASE        BK7258_AON_GPIO_BASE

#define BK7258_GPIO_CFG(n)      (BK7258_GPIO_BASE + (n) * 4)
#define BK7258_GPIO_DATA        (BK7258_GPIO_BASE + 0x100)
#define BK7258_GPIO_DIR         (BK7258_GPIO_BASE + 0x104)

#define BK7258_GPIO_INPUT       0x00
#define BK7258_GPIO_OUTPUT      0x01
#define BK7258_GPIO_PULLUP      0x02
#define BK7258_GPIO_PULLDOWN    0x03

#define BK7258_GPIO_NUM         48

/****************************************************************************
 * Flash 控制器寄存器 (SDK确认)
 * 基地址: 0x44030000 (SOC_FLASH_REG_BASE)
 ****************************************************************************/

#define BK7258_FLASH_CMD        (BK7258_FLASH_CTRL_BASE + 0x00)
#define BK7258_FLASH_ADDR       (BK7258_FLASH_CTRL_BASE + 0x04)
#define BK7258_FLASH_DATA       (BK7258_FLASH_CTRL_BASE + 0x08)
#define BK7258_FLASH_STATUS     (BK7258_FLASH_CTRL_BASE + 0x0C)
#define BK7258_FLASH_CTRL       (BK7258_FLASH_CTRL_BASE + 0x10)

#define BK7258_FLASH_CMD_READ   0x00
#define BK7258_FLASH_CMD_PROG   0x01
#define BK7258_FLASH_CMD_ERASE  0x02
#define BK7258_FLASH_CMD_WREN   0x03

#define BK7258_FLASH_STS_BUSY   (1 << 0)
#define BK7258_FLASH_STS_ERR    (1 << 1)

/* Flash 分区表 */
#define BK7258_FLASH_BOOT_OFFSET    0x000000   /* Bootloader  256KB */
#define BK7258_FLASH_KERNEL_OFFSET  0x040000   /* Kernel      1MB   */
#define BK7258_FLASH_ROOTFS_OFFSET  0x140000   /* RootFS      2MB   */
#define BK7258_FLASH_AIMODEL_OFFSET 0x340000   /* AI 模型     2MB   */
#define BK7258_FLASH_USER_OFFSET    0x540000   /* 用户数据    1MB   */

#define BK7258_FLASH_BOOT_SIZE      0x040000
#define BK7258_FLASH_KERNEL_SIZE    0x100000
#define BK7258_FLASH_ROOTFS_SIZE    0x200000
#define BK7258_FLASH_AIMODEL_SIZE   0x200000
#define BK7258_FLASH_USER_SIZE      0x100000

/****************************************************************************
 * 定时器寄存器 (SDK确认)
 * 基地址: TIMER0=0x44810000, TIMER1=0x45800000
 ****************************************************************************/

#define BK7258_TIMER_CTRL      0x00
#define BK7258_TIMER_LOAD      0x04
#define BK7258_TIMER_VALUE     0x08
#define BK7258_TIMER_INTCLR    0x0C
#define BK7258_TIMER_RIS       0x10
#define BK7258_TIMER_MIS       0x14

#define BK7258_TIMER_CTRL_EN   (1 << 0)
#define BK7258_TIMER_CTRL_MODE (1 << 1)        /* 1=周期 0=单次 */
#define BK7258_TIMER_CTRL_INT  (1 << 2)        /* 中断使能 */

/****************************************************************************
 * I2S / 音频寄存器 (SDK确认)
 * 基地址: I2S0=0x47810000, I2S1=0x47820000, I2S2=0x47830000
 *         AUDIO_DSP=0x47800000 (SOC_AUD_REG_BASE)
 ****************************************************************************/

#define BK7258_I2S_CTRL        0x00
#define BK7258_I2S_FIFO        0x04
#define BK7258_I2S_FMT         0x08
#define BK7258_I2S_SRATE       0x0C
#define BK7258_I2S_INTEN       0x10
#define BK7258_I2S_INTSTS      0x14
#define BK7258_I2S_RXDMA       0x18
#define BK7258_I2S_TXDMA       0x1C

#define BK7258_I2S_CTRL_ENABLE (1 << 0)
#define BK7258_I2S_CTRL_RX     (1 << 1)
#define BK7258_I2S_CTRL_TX     (1 << 2)
#define BK7258_I2S_CTRL_DMA    (1 << 3)
#define BK7258_I2S_CTRL_MCLK   (1 << 4)

#define BK7258_I2S_FMT_I2S     0x00
#define BK7258_I2S_FMT_LEFT    0x01
#define BK7258_I2S_FMT_RIGHT   0x02

/* 音频 DSP 寄存器 */
#define BK7258_DSP_CTRL        0x00
#define BK7258_DSP_NR_LEVEL    0x04            /* 降噪等级 */
#define BK7258_DSP_AEC_ENABLE  0x08            /* 回声消除使能 */
#define BK7258_DSP_AGC_LEVEL   0x0C            /* 自动增益等级 */

/****************************************************************************
 * LCD 寄存器 (SDK确认)
 * 基地址: 0x48060000 (SOC_LCD_DISP_REG_BASE)
 ****************************************************************************/

#define BK7258_LCD_CTRL         0x00
#define BK7258_LCD_FB_ADDR      0x04
#define BK7258_LCD_WIDTH        0x08
#define BK7258_LCD_HEIGHT       0x0C
#define BK7258_LCD_STRIDE       0x10
#define BK7258_LCD_FORMAT       0x14
#define BK7258_LCD_INTEN        0x18
#define BK7258_LCD_INTSTS       0x1C

#define BK7258_LCD_CTRL_ENABLE  (1 << 0)
#define BK7258_LCD_CTRL_2D      (1 << 1)       /* 2D 加速使能 */
#define BK7258_LCD_CTRL_VSYNC   (1 << 2)       /* VSYNC 中断 */

#define BK7258_LCD_FMT_RGB565   0x00
#define BK7258_LCD_FMT_RGB888   0x01

/* 2D 图形加速寄存器 (DMA2D, 基地址: 0x48080000) */
#define BK7258_2D_BLIT_SRC      0x20            /* 源地址 */
#define BK7258_2D_BLIT_DST      0x24            /* 目标地址 */
#define BK7258_2D_BLIT_W        0x28            /* 宽度 */
#define BK7258_2D_BLIT_H        0x2C            /* 高度 */
#define BK7258_2D_BLIT_CTRL     0x30            /* Blit 控制 */
#define BK7258_2D_BLIT_START    (1 << 0)

/****************************************************************************
 * AI 加速器寄存器 (待确认 - BK7258 AI 加速器基地址未在 SDK reg_base.h 中公开)
 ****************************************************************************/

#define BK7258_AI_BASE          0x4000F000

#define BK7258_AI_CTRL          0x00
#define BK7258_AI_STATUS        0x04
#define BK7258_AI_WEIGHT_ADDR   0x08
#define BK7258_AI_INPUT_ADDR    0x0C
#define BK7258_AI_OUTPUT_ADDR   0x10
#define BK7258_AI_LAYER         0x14            /* 当前层索引 */
#define BK7258_AI_MODE          0x18            /* 推理模式 */
#define BK7258_AI_INTEN         0x1C
#define BK7258_AI_INTSTS        0x20
#define BK7258_AI_ERR           0x24

#define BK7258_AI_CTRL_START    (1 << 0)
#define BK7258_AI_CTRL_RESET    (1 << 1)
#define BK7258_AI_CTRL_INTEN    (1 << 2)

#define BK7258_AI_STATUS_BUSY   (1 << 0)
#define BK7258_AI_STATUS_DONE   (1 << 1)
#define BK7258_AI_STATUS_ERR    (1 << 2)

#define BK7258_AI_MODE_INT8     0x00            /* int8 量化推理 */
#define BK7258_AI_MODE_INT16    0x01            /* int16 量化推理 */

/****************************************************************************
 * WiFi / BLE 寄存器 (SDK确认)
 * 基地址: 0x4A800000 (SOC_XVR_REG_BASE - WiFi/BLE XVR)
 ****************************************************************************/

#define BK7258_WIFI_BASE        BK7258_XVR_BASE
#define BK7258_BLE_BASE         (BK7258_XVR_BASE + 0x4000)

#define BK7258_WIFI_CTRL        0x00
#define BK7258_WIFI_TXDESC      0x04
#define BK7258_WIFI_RXDESC      0x08
#define BK7258_WIFI_INTEN       0x0C
#define BK7258_WIFI_INTSTS      0x10

#define BK7258_WIFI_CTRL_ENABLE (1 << 0)
#define BK7258_WIFI_CTRL_TX     (1 << 1)
#define BK7258_WIFI_CTRL_RX     (1 << 2)

/****************************************************************************
 * 中断号定义 (SDK确认)
 * 来源: bk_idk/middleware/soc/bk7258/interrupts.h
 ****************************************************************************/

#define BK7258_IRQ_DMA0         0               /* DMA0 */
#define BK7258_IRQ_ENC_SEC      1               /* ENC_SEC */
#define BK7258_IRQ_ENC_NSEC     2               /* ENC_NSEC */
#define BK7258_IRQ_TIMER0       3               /* TIMER0 */
#define BK7258_IRQ_UART0        4               /* UART0 */
#define BK7258_IRQ_PWM0         5               /* PWM0 */
#define BK7258_IRQ_I2C0         6               /* I2C0 */
#define BK7258_IRQ_SPI0         7               /* SPI0 */
#define BK7258_IRQ_SARADC       8               /* SARADC */
#define BK7258_IRQ_IRDA         9               /* IRDA */
#define BK7258_IRQ_SDIO         10              /* SDIO */
#define BK7258_IRQ_GDMA         11              /* GDMA */
#define BK7258_IRQ_LA           12              /* LA */
#define BK7258_IRQ_TIMER1       13              /* TIMER1 */
#define BK7258_IRQ_I2C1         14              /* I2C1 */
#define BK7258_IRQ_UART1        15              /* UART1 */
#define BK7258_IRQ_UART2        16              /* UART2 */
#define BK7258_IRQ_SPI1         17              /* SPI1 */
#define BK7258_IRQ_CAN          18              /* CAN */
#define BK7258_IRQ_USB          19              /* USB */
#define BK7258_IRQ_QSPI0        20              /* QSPI0 */
#define BK7258_IRQ_CKMN_FFT     21              /* CKMN(FFT) */
#define BK7258_IRQ_SBC          22              /* SBC */
#define BK7258_IRQ_AUD          23              /* AUD */
#define BK7258_IRQ_I2S0         24              /* I2S0 */
#define BK7258_IRQ_JPEGENC      25              /* JPEGENC */
#define BK7258_IRQ_JPEGDEC      26              /* JPEGDEC */
#define BK7258_IRQ_LCD          27              /* LCD */
#define BK7258_IRQ_DMA2D        28              /* DMA2D */
#define BK7258_IRQ_PHY_MBP      29              /* PHY_MBP */
#define BK7258_IRQ_PHY_RIU      30              /* PHY_RIU */
#define BK7258_IRQ_MAC_TX_RX_TIMER  31         /* MAC_TX_RX_TIMER */
#define BK7258_IRQ_MAC_TX_RX_MISC   32         /* MAC_TX_RX_MISC */
#define BK7258_IRQ_MAC_RX_TRIGGER   33         /* MAC_RX_TRIGGER */
#define BK7258_IRQ_MAC_TX_TRIGGER   34         /* MAC_TX_TRIGGER */
#define BK7258_IRQ_MAC_PORT_TRIGGER  35        /* MAC_PORT_TRIGGER */
#define BK7258_IRQ_MAC_GEN      36              /* MAC_GEN */
#define BK7258_IRQ_HSU_IRQ      37              /* HSU_IRQ */
#define BK7258_IRQ_MAC_WAKEUP   38              /* MAC_WAKEUP */
#define BK7258_IRQ_DM           39              /* DM */
#define BK7258_IRQ_BLE          40              /* BLE */
#define BK7258_IRQ_BT           41              /* BT */
#define BK7258_IRQ_QSPI1        42              /* QSPI1 */
#define BK7258_IRQ_PWM1         43              /* PWM1 */
#define BK7258_IRQ_I2S1         44              /* I2S1 */
#define BK7258_IRQ_I2S2         45              /* I2S2 */
#define BK7258_IRQ_H264         46              /* H264 */
#define BK7258_IRQ_SDMADC       47              /* SDMADC */
#define BK7258_IRQ_MBOX0        48              /* MBOX0 */
#define BK7258_IRQ_MBOX1        49              /* MBOX1 */
#define BK7258_IRQ_BMC64        50              /* BMC64 */
#define BK7258_IRQ_DPLL_UNLOCK  51              /* DPLL_UNLOCK */
#define BK7258_IRQ_TOUCHED      52              /* TOUCHED */
#define BK7258_IRQ_USBPLUG      53              /* USBPLUG */
#define BK7258_IRQ_RTC          54              /* RTC */
#define BK7258_IRQ_GPIO         55              /* GPIO */
#define BK7258_IRQ_DMA1_SEC     56              /* DMA1_SEC */
#define BK7258_IRQ_DMA1_NSEC    57              /* DMA1_NSEC */
#define BK7258_IRQ_YUVB         58              /* YUVB */
#define BK7258_IRQ_ROTT         59              /* ROTT */
#define BK7258_IRQ_INT_ID_MAX   60              /* INT_ID_MAX */

/****************************************************************************
 * 公共函数声明
 ****************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************
 * 各驱动初始化函数
 ****************************************************************************/

/* 时钟 */
void bk7258_clock_config(uint32_t freq);

/* UART */
int bk7258_uart_initialize(int port);

/* Flash */
struct mtd_dev_s;
struct mtd_dev_s *bk7258_flash_initialize(void);

/* GPIO */
int bk7258_gpio_initialize(void);
void bk7258_gpio_config(int port, int mode);
void bk7258_gpio_write(int port, bool value);
bool bk7258_gpio_read(int port);

/* 定时器 */
int bk7258_timer_initialize(void);

/* I2S / 音频 */
struct i2s_dev_s;
struct i2s_dev_s *bk7258_i2s_initialize(int port);
int bk7258_audio_dsp_configure(uint32_t srate, uint8_t channels);

/* WiFi */
int bk7258_wlan_initialize(void);

/* LCD */
int bk7258_lcd_initialize(void);
int bk7258_touch_initialize(void);
void *bk7258_lcd_get_framebuffer(void);

/* AI 加速器 */
int bk7258_ai_accel_initialize(void);
int bk7258_ai_infer(const void *input, void *output, uint32_t size);

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_CHIP_BK7258_H */
