#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "../device/timer.h"
#include "../kernel/memory.h"
#include "../thread/thread.h"
#include "../device/console.h"
#include "../device/keyboard.h"
#include "../userprog/tss.h"
#include "../userprog/syscall_init.h"
#include "ide.h"
#include "fs.h"

//初始化所有子模块
void init_all() {
      put_str("init_all\n");
      idt_init();		      // 初始化中断
      // timer_init();		// 初始化PIT
      mem_init();		      // 初始化内存管理 
      thread_init();		// 初始化线程管理
      timer_init();		// 初始化PIT
      console_init();		// 初始化控制台
      keyboard_init();	      // 初始化键盘
      tss_init();             // 初始化tss
      syscall_init();		// 初始化系统调用
      intr_enable();	      // 开启中断ide需要
      ide_init();		      // 初始化硬盘
      filesys_init();	      // 初始化文件系统
}