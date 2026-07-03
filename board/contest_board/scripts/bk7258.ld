/****************************************************************************
 * board/contest_board/scripts/bk7258.ld
 *
 * BK7258 链接脚本 (Cortex-M33, ARMv8-M)
 *
 * 内存布局:
 *   FLASH (code + rodata):  Flash @ 0x00000000
 *   SRAM  (data + bss):     640KB SRAM @ 0x20000000
 *   PSRAM (heap + stack):   16MB PSRAM @ 0x24000000
 *
 * 注意: 具体 ORIGIN/LENGTH 需对照 BK7258 数据手册确认。
 *
 ****************************************************************************/

OUTPUT_ARCH(arm)
EXTERN(_vectors)
ENTRY(_stext)

MEMORY
{
  /* Flash: 存放代码与只读数据 */
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M

  /* 片内 SRAM: 存放 data/bss, 高速访问 */
  SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 640K

  /* 外挂 PSRAM: 主堆区, 适合 AI 模型与大缓冲 */
  PSRAM (rwx) : ORIGIN = 0x24000000, LENGTH = 16M
}

/****************************************************************************
 * 栈大小配置
 ****************************************************************************/

/* 主栈 (MSP), 用于中断与启动阶段 */
__main_stack_size = 0x1000;     /* 4KB */

/* 进程栈 (PSP), 用于线程 */
__proc_stack_size = 0x2000;     /* 8KB */

/* 堆大小 (PSRAM 中分配, 剩余空间全部给堆) */
__heap_size = 16M - 0x10000;    /* 16MB - 64KB */

/****************************************************************************
 * 段定义
 ****************************************************************************/

SECTIONS
{
  /* 向量表必须放在 Flash 起始位置 */
  .vectors :
  {
    _stext = .;
    KEEP(*(.vectors))
    . = ALIGN(0x100);
  } > FLASH

  /* 代码段 */
  .text :
  {
    *(.text .text.*)
    *(.gnu.warning)
    *(.glue_7)
    *(.glue_7t)
    *(.rodata .rodata.*)
    . = ALIGN(4);
  } > FLASH

  /* ARM 异常处理表 */
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  /* 构造/析构函数表 */
  .init_section :
  {
    _sinit = ABSOLUTE(.);
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    _einit = ABSOLUTE(.);
  } > FLASH

  _etext = .;

  /* data 段: 加载在 Flash, 运行时拷贝到 SRAM */
  .data :
  {
    _sdata = ABSOLUTE(.);
    *(.data .data.*)
    . = ALIGN(4);
    _edata = ABSOLUTE(.);
  } > SRAM AT > FLASH

  /* bss 段: 运行时清零 */
  .bss (NOLOAD) :
  {
    _sbss = ABSOLUTE(.);
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = ABSOLUTE(.);
  } > SRAM

  /* 主栈 (MSP) */
  .stack (NOLOAD) :
  {
    _sstack = ABSOLUTE(.);
    . = ALIGN(8);
    . = . + __main_stack_size;
    _estack = ABSOLUTE(.);
  } > SRAM

  /* 堆区 (PSRAM) */
  .heap (NOLOAD) :
  {
    _sheap = ABSOLUTE(.);
    . = ALIGN(8);
    . = . + __heap_size;
    _eheap = ABSOLUTE(.);
  } > PSRAM

  /* AI 加速器专用 SRAM (保留, 不参与堆分配) */
  .ai_sram (NOLOAD) :
  {
    _sai_sram = ABSOLUTE(.);
    . = . + 128K;
    _eai_sram = ABSOLUTE(.);
  } > SRAM
}

/****************************************************************************
 * 符号导出 (供启动代码使用)
 ****************************************************************************/

PROVIDE(_stext       = _stext);
PROVIDE(_etext       = _etext);
PROVIDE(_sdata       = _sdata);
PROVIDE(_edata       = _edata);
PROVIDE(_sbss        = _sbss);
PROVIDE(_ebss        = _ebss);
PROVIDE(_sheap       = _sheap);
PROVIDE(_eheap       = _eheap);
PROVIDE(_sstack      = _sstack);
PROVIDE(_estack      = _estack);
PROVIDE(_sai_sram    = _sai_sram);
PROVIDE(_eai_sram    = _eai_sram);
