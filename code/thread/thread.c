#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "memory.h"
#include "../userprog/process.h"

#define PG_SIZE 4096

struct task_struct* main_thread;    //主线程PCB
struct list thread_ready_list;     //可运行队列
struct list thread_all_list;        //全部队列
static struct list_elem* thread_tag;    //线程标签

extern void switch_to(struct task_struct* cur, struct task_struct* next);

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
    strcpy(pthread->name, name);

    if(pthread == main_thread){
      //把main函数也封装成一个线程，并且他是一直运行的，所以将其设置为 TASK_RUNNING
        pthread->status = TASK_RUNNING;
    }
    else{
        pthread->status = TASK_READY;
    }

    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
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

void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    //将当前main函数创建为线程
    make_main_thread();
    put_str("thread_init end\n");
}