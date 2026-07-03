/****************************************************************************
 * board/contest_board/src/bk7258_vectors.S
 *
 * BK7258 启动向量表与异常处理 (ARM Cortex-M33)
 *
 * 包含:
 *   - 向量表 (初始栈指针 + 异常向量)
 *   - 复位处理函数 (_start)
 *   - 默认异常处理
 *
 * 注意: 实际启动代码由 NuttX 提供 (arch/arm/src/armv8-m),
 *       此文件定义 BK7258 专属的向量表入口。
 *
 ****************************************************************************/

    .syntax unified
    .thumb

    .section .vectors, "ax"
    .align 8

/****************************************************************************
 * 向量表
 ****************************************************************************/

    .global _vectors
_vectors:
    .word _estack                  /* 0: 初始栈指针 (MSP) */
    .word _start                   /* 1: 复位 */
    .word bk7258_nmi_handler       /* 2: NMI */
    .word bk7258_hardfault_handler /* 3: HardFault */
    .word bk7258_memfault_handler  /* 4: MemFault */
    .word bk7258_busfault_handler  /* 5: BusFault */
    .word bk7258_usagefault_handler /* 6: UsageFault */
    .word 0                         /* 7: Reserved */
    .word 0                         /* 8: Reserved */
    .word 0                         /* 9: Reserved */
    .word 0                         /* 10: Reserved */
    .word bk7258_svcall_handler    /* 11: SVCall */
    .word bk7258_debugmon_handler  /* 12: Debug Monitor */
    .word 0                         /* 13: Reserved */
    .word bk7258_pendsv_handler    /* 14: PendSV */
    .word bk7258_systick_handler   /* 15: SysTick */

    /* 外设中断向量 (16+) */
    .word bk7258_default_handler   /* 16: IRQ0 */
    .word bk7258_default_handler   /* 17: IRQ1 */
    .word bk7258_default_handler   /* 18: IRQ2 */
    .word bk7258_default_handler   /* 19: IRQ3 */
    .word bk7258_uart0_handler     /* 20: UART0 */
    .word bk7258_uart1_handler     /* 21: UART1 */
    .word bk7258_default_handler   /* 22-27: Reserved */
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_timer0_handler    /* 28: Timer0 */
    .word bk7258_timer1_handler    /* 29: Timer1 */
    .word bk7258_i2s0_handler      /* 30: I2S0 */
    .word bk7258_default_handler   /* 31-39: Reserved */
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_lcd_handler       /* 40: LCD */
    .word bk7258_default_handler   /* 41-49: Reserved */
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_default_handler
    .word bk7258_ai_handler        /* 50: AI 加速器 */

    .size _vectors, . - _vectors

/****************************************************************************
 * 默认中断处理函数
 ****************************************************************************/

    .section .text, "ax"
    .align 2

    .weak bk7258_default_handler
bk7258_default_handler:
    b bk7258_default_handler

    .weak bk7258_nmi_handler
bk7258_nmi_handler:
    b bk7258_nmi_handler

    .weak bk7258_hardfault_handler
bk7258_hardfault_handler:
    b bk7258_hardfault_handler

    .weak bk7258_memfault_handler
bk7258_memfault_handler:
    b bk7258_memfault_handler

    .weak bk7258_busfault_handler
bk7258_busfault_handler:
    b bk7258_busfault_handler

    .weak bk7258_usagefault_handler
bk7258_usagefault_handler:
    b bk7258_usagefault_handler

    .weak bk7258_svcall_handler
bk7258_svcall_handler:
    b bk7258_svcall_handler

    .weak bk7258_debugmon_handler
bk7258_debugmon_handler:
    b bk7258_debugmon_handler

    .weak bk7258_pendsv_handler
bk7258_pendsv_handler:
    b bk7258_pendsv_handler

    .weak bk7258_systick_handler
bk7258_systick_handler:
    b bk7258_systick_handler

    /* 外设中断弱定义 */
    .weak bk7258_uart0_handler
bk7258_uart0_handler:
    b bk7258_uart0_handler

    .weak bk7258_uart1_handler
bk7258_uart1_handler:
    b bk7258_uart1_handler

    .weak bk7258_timer0_handler
bk7258_timer0_handler:
    b bk7258_timer0_handler

    .weak bk7258_timer1_handler
bk7258_timer1_handler:
    b bk7258_timer1_handler

    .weak bk7258_i2s0_handler
bk7258_i2s0_handler:
    b bk7258_i2s0_handler

    .weak bk7258_lcd_handler
bk7258_lcd_handler:
    b bk7258_lcd_handler

    .weak bk7258_ai_handler
bk7258_ai_handler:
    b bk7258_ai_handler
