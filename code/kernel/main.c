#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall_init.h"
#include "syscall.h"
#include "stdio.h"
#include "memory.h"
#include "dir.h"
#include "fs.h"
#include "assert.h"
#include "shell.h"

#include "ide.h"
#include "stdio_kernel.h"

void init(void);

int main(void) {
   put_str("I am kernel\n");
   init_all();

/*************    写入应用程序    *************/
//   uint32_t file_size = 5343; 
//   uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
//   struct disk* sda = &channels[0].devices[0];
//   void* prog_buf = sys_malloc(file_size);
//   ide_read(sda, 300, prog_buf, sec_cnt);
//   int32_t fd = sys_open("/prog_pipe", O_CREAT|O_RDWR);
//   if (fd != -1) {
//      if(sys_write(fd, prog_buf, file_size) == -1) {
//         printk("file write error!\n");
//         while(1);
//      }
//   }
/*************    写入应用程序结束   *************/
   cls_screen();
   console_put_str("[rabbit@localhost /]$ ");
   thread_exit(running_thread(), true);
   return 0;
}

/* init进程 */
void init(void) {
   uint32_t ret_pid = fork();
   if(ret_pid) {  // 父进程
      int status;
      int child_pid;
       /* init在此处不停的回收僵尸进程 */
       while(1) {
	  child_pid = wait(&status);
	  printf("I`m init, My pid is 1, I recieve a child, It`s pid is %d, status is %d\n", child_pid, status);
       }
   } else {	  // 子进程
      my_shell();
   }
   panic("init: should not be here");
}

// #include "print.h"
// #include "init.h"
// #include "debug.h"
// #include "thread.h"
// #include "console.h"
// #include "interrupt.h"
// #include "ioqueue.h"
// #include "keyboard.h"
// #include "process.h"
// #include "syscall.h"
// #include "syscall-init.h"
// #include "../lib/stdio.h"
// #include "memory.h"
// #include "fs.h"
// #include "string.h"
// #include "shell.h"


// void k_thread_a(void*);
// void k_thread_b(void*);
// void u_prog_a(void);
// void u_prog_b(void);

// int main(void) {

//    // put_str("I am kernel\n");
//    // init_all();
//    // process_execute(u_prog_a, "u_prog_a");
//    // process_execute(u_prog_b, "u_prog_b");
//    // thread_start("k_thread_a", 31, k_thread_a, "I am thread_a");
//    // thread_start("k_thread_b", 31, k_thread_b, "I am thread_b");
//    // sys_open("/file1", O_CREAT);
//    // while(1);
//    // return 0;

//    put_str("I am kernel\n");
//    init_all();

//    uint32_t fd = sys_open("/file1", O_RDWR);
//    printf("fd:%d\n", fd);
//    sys_write(fd, "hello,world\n", 12);
//    sys_close(fd);
//    printf("%d closed now\n", fd);
//    while(1);
//    return 0;
// }

// /* 在线程中运行的函数 */
// void k_thread_a(void* arg) {     
//    void* addr1 = sys_malloc(256);
//    void* addr2 = sys_malloc(255);
//    void* addr3 = sys_malloc(254);
//    console_put_str(" thread_a malloc addr:0x");
//    console_put_int((int)addr1);
//    console_put_char(',');
//    console_put_int((int)addr2);
//    console_put_char(',');
//    console_put_int((int)addr3);
//    console_put_char('\n');

//    int cpu_delay = 100000;
//    while(cpu_delay-- > 0);
//    sys_free(addr1);
//    sys_free(addr2);
//    sys_free(addr3);
//    while(1);
// }

// /* 在线程中运行的函数 */
// void k_thread_b(void* arg) {     
//    void* addr1 = sys_malloc(256);
//    void* addr2 = sys_malloc(255);
//    void* addr3 = sys_malloc(254);
//    console_put_str(" thread_b malloc addr:0x");
//    console_put_int((int)addr1);
//    console_put_char(',');
//    console_put_int((int)addr2);
//    console_put_char(',');
//    console_put_int((int)addr3);
//    console_put_char('\n');

//    int cpu_delay = 100000;
//    while(cpu_delay-- > 0);
//    sys_free(addr1);
//    sys_free(addr2);
//    sys_free(addr3);
//    while(1);
// }

// /* 测试用户进程 */
// void u_prog_a(void) {
//    void* addr1 = malloc(256);
//    void* addr2 = malloc(255);
//    void* addr3 = malloc(254);
//    printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

//    int cpu_delay = 100000;
//    while(cpu_delay-- > 0);
//    free(addr1);
//    free(addr2);
//    free(addr3);
//    while(1);
// }

// /* 测试用户进程 */
// void u_prog_b(void) {
//    void* addr1 = malloc(256);
//    void* addr2 = malloc(255);
//    void* addr3 = malloc(254);
//    printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

//    int cpu_delay = 100000;
//    while(cpu_delay-- > 0);
//    free(addr1);
//    free(addr2);
//    free(addr3);
//    while(1);
// }