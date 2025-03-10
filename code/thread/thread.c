#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "memory.h"
#include "../userprog/process.h"
#include "sync.h"
#include "stdio.h"
#include "file.h"
#include "fs.h"

/* pid的位图,最大支持1024个pid */
uint8_t pid_bitmap_bits[128] = {0};

/* pid池 */
struct pid_pool {
   struct bitmap pid_bitmap;  // pid位图
   uint32_t pid_start;	      // 起始pid
   struct lock pid_lock;      // 分配pid锁
}pid_pool;

#define PG_SIZE 4096

struct lock pid_lock;        //分配pid锁

struct task_struct* idle_thread;    //idle线程

struct task_struct* main_thread;    //主线程PCB
struct list thread_ready_list;     //可运行队列
struct list thread_all_list;        //全部队列
static struct list_elem* thread_tag;    //线程标签

extern void switch_to(struct task_struct* cur, struct task_struct* next);
extern void init(void);

//系统空闲运行的线程
static void idle(void* arg UNUSED) {
    while(1){
        thread_block(TASK_BLOCKED);
        //执行hlt时必须要保证目前在开中断的情况下
        asm volatile ("sti; hlt" : : : "memory");
    }
}

//获取当前线程pcb指针
struct task_struct* running_thread () {
    uint32_t esp;
    asm ("mov %%esp, %0" : "=g"(esp));
    //取esp整数部分即pcb起始地址
    return (struct task_struct*)(esp & 0xfffff000);
}

//由kernel_thread去执行function(func_arg)
static void kernel_thread(thread_func* function, void* func_arg) {
    //执行function前要开中断，避免后面的时钟中断被屏蔽，无法调度其他线程
    intr_enable();
    function(func_arg);
}

/* 初始化pid池 */
static void pid_pool_init(void) { 
   pid_pool.pid_start = 1;
   pid_pool.pid_bitmap.bits = pid_bitmap_bits;
   pid_pool.pid_bitmap.btmp_bytes_len = 128;
   bitmap_init(&pid_pool.pid_bitmap);
   lock_init(&pid_pool.pid_lock);
}

//分配pid
static pid_t allocate_pid(void) {
    // static pid_t nextpid = 0;
    // lock_acquire(&pid_lock);
    // nextpid++;
    // lock_release(&pid_lock);
    // return nextpid;
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
    lock_release(&pid_pool.pid_lock);
    return (bit_idx + pid_pool.pid_start);
}

/* 释放pid */
void release_pid(pid_t pid) {
   lock_acquire(&pid_pool.pid_lock);
   int32_t bit_idx = pid - pid_pool.pid_start;
   bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
   lock_release(&pid_pool.pid_lock);
}


//fork进程时为其分配pid，因为allocate_pid已经是静态的，别的文件无法调用，
//不想改变函数定义，在这里使用pid_fork函数来封装一下
pid_t fork_pid(void) {
    return allocate_pid();
}

//初始化线程栈thread_stack       将待执行的函数和参数放到线程栈中相应的位置
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {
    //先预留中断使用的栈空间
    pthread->self_kstack -= sizeof(struct intr_stack);

    //留出线程空间
    pthread->self_kstack -= sizeof(struct thread_stack);
    struct thread_stack* kthread_stack = (struct thread_stack* )pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

//初始化线程基本信息
void init_thread(struct task_struct* pthread, char* name, int prio) {
//     memset(pthread, 0, sizeof(struct task_struct));        //初始化线程结构体， 置0
//     strcpy(pthread->name, name);                           //设置线程名
//     pthread->status = TASK_RUNNING;                       //设置线程状态
//     pthread->priority = prio;                            //设置线程优先级
// // self_kstack是线程自己在内核态下使用的栈顶地址
//     pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
//     pthread->stack_magic = 0x19870916;                    //栈的魔数，用于检测栈是否溢出
    memset(pthread, 0, sizeof(struct task_struct));        //初始化线程结构体， 置0
    pthread->pid = allocate_pid();
    strcpy(pthread->name, name);

    if(pthread == main_thread){
      //把main函数也封装成一个线程，并且他是一直运行的，所以将其设置为 TASK_RUNNING
        pthread->status = TASK_RUNNING;
    }
    else{
        pthread->status = TASK_READY;
    }

    //self_kstack是线程自己在内核态下使用的栈顶地址
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;

    //预留标准输入、输出、错误
    pthread->fd_table[0] = 0;
    pthread->fd_table[1] = 1;
    pthread->fd_table[2] = 2;
    //其余全置为-1
    uint8_t fd_idx = 3;
    while(fd_idx < MAX_FILES_OPEN_PER_PROC){
        pthread->fd_table[fd_idx] = -1;
        fd_idx++;
    }
    
    pthread->cwd_inode_nr = 0;  //根目录
    pthread->parent_pid = -1;   //父进程默认为-1,表示没有父进程
    pthread->stack_magic = 0x19870916;
}

//创建一优先级prio的线程，线程名为name,线程所执行的函数是function(func_arg)
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {
//pcb都位于内核空间，包括用户进程的pcb也是在内核空间
    // struct task_struct* thread = get_kernel_pages(1);    //获取一页物理内存作为线程的内核栈

    // init_thread(thread, name, prio);                     //初始化线程基本信息
    // thread_create(thread, function, func_arg);           //创建线程

    // asm volatile ("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret" : : "g" (thread->self_kstack) : "memory");
    // return thread;
    struct task_struct* thread = get_kernel_pages(1);    //获取一页物理内存作为线程的内核栈

    init_thread(thread,name,prio);                     //初始化线程基本信息
    thread_create(thread,function,func_arg);           //创建线程

    //确保之前不在队列中
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));

    //加入就绪线程队列
    list_append(&thread_ready_list,&thread->general_tag); //将线程加入到就绪队列

    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    //加入全部线程队列
    list_append(&thread_all_list,&thread->all_list_tag);
    return thread;
}

//将kernel中的main函数完善为主线程
static void make_main_thread(void) {
    //main线程早已运行，在mov esp,0xc009f000,就是为其预留了tcb。
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);

    //main函数是当前线程，当前线程不在thread_ready_list中
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

void schedule(){
    ASSERT(intr_get_status() == INTR_OFF);

    struct task_struct* cur = running_thread();
    if(cur->status == TASK_RUNNING){ //若线程只是CPU时间片到了，将其加入到就绪列队尾
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur->ticks = cur->priority;
        cur->status = TASK_READY; //将当前线程状态设置为就绪
    }
    else{

    }

    //如果就绪队列中没有可运行的任务，就唤醒idle
    if(list_empty(&thread_ready_list)) {
        thread_unblock(idle_thread);
    }

    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    //从就绪队列中取出一个线程
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;

    //激活任务页表
    process_activate(next);

    switch_to(cur, next);
}

//当前线程将自己阻塞，标志其状态为stat
void thread_block(enum task_status stat) {
	//stat取值为TASK_BLOCKED,TASK_WAITING,TASK_HANGING,也就是这三种状态才不会被调度
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));
    enum intr_status old_status = intr_disable();
    struct task_struct* cur = running_thread();
    cur->status = stat;			//置其状态为stat
    schedule();					//将当前线程换下处理器
	//待当前线程被解除阻塞后才继续运行下面的intr_set_status
    intr_set_status(old_status);
}

//将线程pthread解除阻塞
void thread_unblock(struct task_struct* pthread) {
    enum intr_status old_status = intr_disable();
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));
    if(pthread->status != TASK_READY) {
        ASSERT(!elem_find(&thread_ready_list,&pthread->general_tag));
        if(elem_find(&thread_ready_list, &pthread->general_tag)){
            PANIC("thread_unblock: blocked thread in ready list."); //阻塞的线程不应该在就绪队列中
        }
        list_push(&thread_ready_list, &pthread->general_tag);       //放到队列前面，尽快得到调度
        pthread->status = TASK_READY;
    }
    intr_set_status(old_status);
}

//主动让出CPU,换其他线程运行
void thread_yield(void) {
    struct task_struct* cur =running_thread();
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->status = TASK_READY;
    schedule();
    intr_set_status(old_status);
}

//以填充空格的方式输出buf
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format)
{
    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0;
    switch (format)
    {
        case 's':
            out_pad_0idx = sprintf(buf, "%s", ptr);
            break;
        case 'd':
            out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));
            break;
        case 'x':
            out_pad_0idx = sprintf(buf, "%x", *((int32_t*)ptr));
    }
    while (out_pad_0idx < buf_len) //填充空格
    {
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }
    sys_write(stdout_no, buf, buf_len-1);
}

//用于在list_traversal函数中的回调函数，用于针对线程队列的处理
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED)
{
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    char out_pad[16];
    memset(out_pad, 0, 16);

    pad_print(out_pad, 16, &pthread->pid, 'd');

    if (pthread->parent_pid == -1) {
        pad_print(out_pad, 16, "NULL", 's');
    } else {
        pad_print(out_pad, 16, &pthread->parent_pid, 'd');
    }

    switch (pthread->status) {
        case 0:
            pad_print(out_pad, 16, "RUNNING", 's');
            break;
        case 1:
            pad_print(out_pad, 16, "READY", 's');
            break;
        case 2:
            pad_print(out_pad, 16, "BLOCKED", 's');
            break;
        case 3:
            pad_print(out_pad, 16, "WAITING", 's');
            break;
        case 4:
            pad_print(out_pad, 16, "HANGING", 's');
            break;
        case 5:
            pad_print(out_pad, 16, "DIED", 's');
    }
    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

    memset(out_pad, 0, 16);
    ASSERT(strlen(pthread->name) < 17);
    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");
    sys_write(stdout_no, out_pad, strlen(out_pad));
    return false;	// 此处返回false是为了迎合主调函数list_traversal,只有回调函数返回false时才会继续调用此函数
}

//打印人物列表
void sys_ps(void)
{
    char* ps_title = "PID            PPID           STAT           TICKS          COMMAND\n";
    sys_write(stdout_no, ps_title, strlen(ps_title));
    list_traversal(&thread_all_list, elem2thread_info, 0);
}

/* 回收thread_over的pcb和页表,并将其从调度队列中去除 */
void thread_exit(struct task_struct* thread_over, bool need_schedule) {
   /* 要保证schedule在关中断情况下调用 */
   intr_disable();
   thread_over->status = TASK_DIED;

   /* 如果thread_over不是当前线程,就有可能还在就绪队列中,将其从中删除 */
   if (elem_find(&thread_ready_list, &thread_over->general_tag)) {
      list_remove(&thread_over->general_tag);
   }
   if (thread_over->pgdir) {     // 如是进程,回收进程的页表
      mfree_page(PF_KERNEL, thread_over->pgdir, 1);
   }

   /* 从all_thread_list中去掉此任务 */
   list_remove(&thread_over->all_list_tag);
   
   /* 回收pcb所在的页,主线程的pcb不在堆中,跨过 */
   if (thread_over != main_thread) {
      mfree_page(PF_KERNEL, thread_over, 1);
   }

   /* 归还pid */
   release_pid(thread_over->pid);

   /* 如果需要下一轮调度则主动调用schedule */
   if (need_schedule) {
      schedule();
      PANIC("thread_exit: should not be here\n");
   }
}

/* 比对任务的pid */
static bool pid_check(struct list_elem* pelem, int32_t pid) {
   struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
   if (pthread->pid == pid) {
      return true;
   }
   return false;
}

/* 根据pid找pcb,若找到则返回该pcb,否则返回NULL */
struct task_struct* pid2thread(int32_t pid) {
   struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
   if (pelem == NULL) {
      return NULL;
   }
   struct task_struct* thread = elem2entry(struct task_struct, all_list_tag, pelem);
   return thread;
}

void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    // lock_init(&pid_lock);
    pid_pool_init();


    //创建init进程
    process_execute(init, "init");

    //将当前main函数创建为线程
    make_main_thread();

    //创建idle线程
    idle_thread = thread_start("idle",10,idle,NULL);

    put_str("thread_init end\n");
}