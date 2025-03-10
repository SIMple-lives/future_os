#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define PIC_M_CTRL 0x20        // 这里用的可变程控制器为8259A.它的主片控制端口为0x20
#define PIC_M_DATA 0x21        // 主片的数据端口为0x21
#define PIC_S_CTRL 0xa0        // 从片控制端口为0xa0
#define PIC_S_DATA 0xa1        // 从片数据端口为0xa1

//#define IDT_DESC_CNT 0x21      // 中断描述符表中的中断个数
//#define IDT_DESC_CNT 0x30		//增加了8259A的中断
#define IDT_DESC_CNT 0x81        //增加0x80号中断描述符，实现系统调用

#define EFLAGS_IF	0x00000200	//eflags寄存器中的if位为1,eflags中IF(9)位为1,则CPU开中断,为0,则CPU关中断
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))

extern uint32_t syscall_handler(void);

/*    中断门描述符结构体    */
struct gate_desc {
   uint16_t    func_offset_low_word;
   uint16_t    selector;
   uint8_t     dcount;   //此项为双字计数字段，是门描述符中的第4字节。此项固定值，不用考虑
   uint8_t     attribute;
   uint16_t    func_offset_high_word;
};

// 静态函数声明，非必需
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];    // idt是中断描述符表

char* intr_name[IDT_DESC_CNT];    // 保存各中断处理程序的名称的字符串数组
/********    定义中断处理程序数组    ********
 * 在kernel.S中定义的intrXXentry只是中断处理程序的入口,
 * 最终调用的是ide_table中的处理程序
 * 定义中断处理程序数组，在kernel.S中定义的intrXXentry只是中断处理程序的入口，最终调用的是ide_table中的处理函数 */
intr_handler idt_table[IDT_DESC_CNT];
/********************************************/

extern intr_handler intr_entry_table[IDT_DESC_CNT];    // 声明引用定义在kernel.S中的中断处理函数数组

/*     初始化可编程中断控制器8259A     */
static void pic_init(void) {

      /* 初始化 主 8259A */
      outb(PIC_M_CTRL, 0x11);   // ICW1: 边沿触发，级联8259,需要ICW4
      outb(PIC_M_DATA, 0x20);   // ICW2: 起始中断向量号为0x20,也就是IR0-7由中断向量0x20-0x27对应
      outb(PIC_M_DATA, 0x04);   // ICW3: IR2对应从片
      outb(PIC_M_DATA, 0x01);   // ICW4: 8086模式,正常EOI

      // 初始化 从 8259A
      outb(PIC_S_CTRL, 0x11);   // ICW1: 边沿触发，级联8259,需要ICW4
      outb(PIC_S_DATA, 0x28);   // ICW2: 起始中断向量号为0x28,也就是IR8-15由中断向量0x28-0x2f对应
      outb(PIC_S_DATA, 0x02);   // ICW3: 对应主8259的IR2引脚
      outb(PIC_S_DATA, 0x01);   // ICW4: 8086模式,正常EOI

      //打开出片上IR0,也就是目前只接受时钟的产生
      //outb(PIC_M_DATA, 0xfe);   //11111110
      //outb(PIC_S_DATA, 0xff);   //11111111

      //测试键盘，只打开键盘中断，其他全部关闭
      //outb(PIC_M_DATA, 0xfd);
      //outb(PIC_S_DATA, 0xff);

      //IRQ2用于级联从片，必须打开，否则无法响应从片上的中断
      //主片上打开的中断有IRQ0的时钟，IRQ1的键盘和级联的IRQ2
      outb(PIC_M_DATA, 0xf8);

      //打开从片的IRQ14,接收硬盘控制器的中断
      outb(PIC_S_DATA, 0xbf);
      put_str("    pic_init done\n");
}

//创建中断门描述符
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) { //三个参数，分别是中断门描述符的指针、中断描述符内的属性及中断描述符内对应的中断处理函数。
      p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
      p_gdesc->selector = SELECTOR_K_CODE;
      p_gdesc->dcount = 0;
      p_gdesc->attribute = attr;
      p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

//初始化中断描述符表
static void idt_desc_init(void) {
      int i;
      int lastindex = IDT_DESC_CNT - 1;
      for(i=0; i< IDT_DESC_CNT; i++) {
            make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
      }
      //单独处理系统调用，系统调用对应的中断门dpl为3
      //中断处理程序为单独的syscall_handler
      make_idt_desc(&idt[lastindex],IDT_DESC_ATTR_DPL3, syscall_handler);
      put_str("    idt_desc_init done\n");
}

//通用的中断处理函数，一般用在异常出现时的处理
static void general_intr_handler(uint8_t vec_nr) {
      if (vec_nr == 0x27 || vec_nr == 0x2f) {      //0x2f是从片8259A上的最后一个irq引脚，保留
            return;                              //IRQ7和IRQ15会产生伪中断(spurious interrupt),无需处理
      }
      //   put_str("int vector: 0x");
      //   put_int(vec_nr);
      //   put_char('\n');
      set_cursor(0); // 将光标设置在屏幕最左上角
      int cursor_pos = 0;
      while (cursor_pos < 320)
      {
         put_char(' ');
         cursor_pos++;
      }

      set_cursor(0); // 将光标设置在屏幕最左上角
      put_str("!!!!!!!      excetion message begin  !!!!!!!!\n");
      set_cursor(88); //从第二行第8个字符开始打印
      put_str(intr_name[vec_nr]);
      if(vec_nr == 14) {     //若为Pagefault,将缺失的地址打印出来并悬停
         int page_fault_vaddr = 0;
         asm("movl %%cr2, %0" : "=r" (page_fault_vaddr));//cr2是存放造成page_fault的虚拟地址
         put_str("\npage fault addr is ");put_int(page_fault_vaddr);
      }
      put_str("\n!!!!!!!      excetion message end    !!!!!!!!\n");
      // 能进入中断处理程序就表示已经处在关中断情况下,
      // 不会出现调度进程的情况。故下面的死循环不会再被中断。
      while(1);
}

/* 完成一般中断处理函数注册及异常名称注册 */
static void exception_init(void) {			    // 完成一般中断处理函数注册及异常名称注册
      int i;
      for (i = 0; i < IDT_DESC_CNT; i++) {

            /* idt_table数组中的函数是在进入中断后根据中断向量号调用的,
             * 见kernel/kernel.S的call [idt_table + %1*4] */
            idt_table[i] = general_intr_handler;		    // 默认为general_intr_handler。
            // 以后会由register_handler来注册具体处理函数。
            intr_name[i] = "unknown";				    // 先统一赋值为unknown
      }
      intr_name[0] = "#DE Divide Error";
      intr_name[1] = "#DB Debug Exception";
      intr_name[2] = "NMI Interrupt";
      intr_name[3] = "#BP Breakpoint Exception";
      intr_name[4] = "#OF Overflow Exception";
      intr_name[5] = "#BR BOUND Range Exceeded Exception";
      intr_name[6] = "#UD Invalid Opcode Exception";
      intr_name[7] = "#NM Device Not Available Exception";
      intr_name[8] = "#DF Double Fault Exception";
      intr_name[9] = "Coprocessor Segment Overrun";
      intr_name[10] = "#TS Invalid TSS Exception";
      intr_name[11] = "#NP Segment Not Present";
      intr_name[12] = "#SS Stack Fault Exception";
      intr_name[13] = "#GP General Protection Exception";
      intr_name[14] = "#PF Page-Fault Exception";
      // intr_name[15] 第15项是intel保留项，未使用
      intr_name[16] = "#MF x87 FPU Floating-Point Error";
      intr_name[17] = "#AC Alignment Check Exception";
      intr_name[18] = "#MC Machine-Check Exception";
      intr_name[19] = "#XF SIMD Floating-Point Exception";

}

/* 开中断并返回开中断前的状态*/
enum intr_status intr_enable() {
   enum intr_status old_status;
   if (INTR_ON == intr_get_status()) {
      old_status = INTR_ON;
      put_str("dsalkdjl\n");
      return old_status;
   } else {
      old_status = INTR_OFF;
      asm volatile("sti");	 // 开中断,sti指令将IF位置1
      return old_status;
   }
}

/* 关中断,并且返回关中断前的状态 */
enum intr_status intr_disable() {
   enum intr_status old_status;
   if (INTR_ON == intr_get_status()) {
      old_status = INTR_ON;
      asm volatile("cli" : : : "memory"); // 关中断,cli指令将IF位置0
      return old_status;
   } else {
      old_status = INTR_OFF;
      return old_status;
   }
}

/* 将中断状态设置为status */
enum intr_status intr_set_status(enum intr_status status) {
   return status & INTR_ON ? intr_enable() : intr_disable();
}

/* 获取当前中断状态 */
enum intr_status intr_get_status() {
   uint32_t eflags = 0;
   GET_EFLAGS(eflags);
   return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

void register_handler(uint8_t vector_no, intr_handler function) {
   idt_table[vector_no] = function; // 注册中断处理函数
}

/*完成有关中断的所有初始化工作*/
void idt_init() {
      put_str("idt_init start\n");
      idt_desc_init();	   // 初始化中断描述符表
      exception_init();	   // 异常名初始化并注册通常的中断处理函数
      pic_init();		   // 初始化8259A

      /* 加载idt */
      uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
      asm volatile("lidt %0" : : "m" (idt_operand));
      put_str("idt_init done\n");
}
