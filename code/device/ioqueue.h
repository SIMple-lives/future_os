#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "../thread/thread.h"
#include "../thread/sync.h"

// #define bufsize 64
#define bufsize 2048	// 兼容管道的一页大小的struct ioqueue

//环形队列
struct ioqueue {
    //生产者消费者问题
    struct lock lock;

    //生产者，缓冲区不满就放数据，满了就阻塞
    struct task_struct* producer;

    //消费者，缓冲区不空就取数据，空了就阻塞
    struct task_struct* consumer;
    char buf[bufsize];    //缓冲区
    int32_t head;
    int32_t tail;
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);
uint32_t ioq_length(struct ioqueue* ioq);
#endif