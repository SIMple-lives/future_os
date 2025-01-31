#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"


void ioqueue_init(struct ioqueue* ioq) {
    lock_init(&ioq->lock);    //初始化锁
    ioq->producer = NULL;    //生产者
    ioq->consumer = NULL;    //消费者
    ioq->head = 0;            //缓冲区头
}

//返回pos在缓冲区中的下一个位置
static int32_t next_pos (int32_t pos) {
    return (pos+1)%bufsize;
}

//bool 判读队列是否已满
bool ioq_full(struct ioqueue* ioq) {
    ASSERT(intr_get_status() == INTR_OFF);
    return next_pos(ioq->head) == ioq->tail;
}

//判断队列是否为空
bool ioq_empty(struct ioqueue* ioq) {
    ASSERT(intr_get_status() == INTR_OFF);
    return ioq->head == ioq->tail;
}

//使生产者在此缓冲区等待
static void ioq_wait(struct task_struct** waiter) {
    ASSERT(*waiter==NULL && waiter!=NULL);
    *waiter = running_thread();
    thread_block(TASK_BLOCKED);
}

/* 唤醒waiter */
static void wakeup(struct task_struct** waiter) {
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}

//消费者从ioq中取一个字符
char ioq_getchar(struct ioqueue* ioq) {
    ASSERT(intr_get_status() == INTR_OFF);

    while(ioq_empty(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);
        lock_release(&ioq->lock);
    }

    char byte = ioq->buf[ioq->tail];
    ioq->tail = next_pos(ioq->tail);

    if(ioq->producer != NULL) {
        wakeup(&ioq->producer);
    }
    return byte;
}

/* 生产者往ioq队列中写入一个字符byte */
void ioq_putchar(struct ioqueue* ioq, char byte) {
    ASSERT(intr_get_status() == INTR_OFF);

    /* 若缓冲区(队列)已经满了,把生产者ioq->producer记为自己,
     * 为的是当缓冲区里的东西被消费者取完后让消费者知道唤醒哪个生产者,
     * 也就是唤醒当前线程自己*/
    while (ioq_full(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }
    ioq->buf[ioq->head] = byte;      // 把字节放入缓冲区中
    ioq->head = next_pos(ioq->head); // 把写游标移到下一位置

    if (ioq->consumer != NULL) {
        wakeup(&ioq->consumer);          // 唤醒消费者
    }
}

uint32_t ioq_length(struct ioqueue* ioq){
    uint32_t len = 0;
    if(ioq->head >= ioq->tail) {
        len = ioq->head - ioq->tail;
    }
    else {
        len = bufsize - (ioq->tail - ioq->head);
    }
    return len;
}