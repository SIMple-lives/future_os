#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include "stdint.h"
#include "../thread/sync.h"
#include "list.h"
#include "bitmap.h"

//分区结构
struct partition {
    uint32_t start_lba;                 //起始扇区
    uint32_t sec_cnt;                   //扇区数
    struct disk* my_disk;               //分区所属的硬盘
    struct list_elem part_tag;          //用于队列中的标记
    char name[8];                       //分区名称
    struct super_block* sb;             //本分区的超级块
    struct bitmap block_bitmap;         //块位图
    struct bitmap inode_bitmap;         //i结点位图
    struct list open_inodes;            //打开的inode链表
};

//硬盘结构
struct disk {
    char name[8];                        //硬盘名称
    struct ide_channel* my_channel;      //此硬盘归属于那个ide通道
    uint8_t dev_no;                      //此硬盘是主0还是从1
    struct partition prim_parts[4];      //主分区顶多4个
    struct partition logic_parts[8];     //逻辑分区理论上无限，支持8个逻辑分区
};

//ata通道结构
struct ide_channel {
    char name[8];                        //通道名称
    uint16_t port_base;                  //通道的起始端口号
    uint8_t irq_no;                      //通道所用的中断号
    struct lock lock;                    //互斥锁
    bool expecting_intr;                 //表示等待硬盘的中断
    struct semaphore disk_done;          //用于阻塞，唤醒驱动程序
    struct disk devices[2];              //此通道上连接的两个硬盘,一主一从
};

void intr_hd_handler(uint8_t irq_no);
void ide_init(void);
extern uint8_t channel_cnt;
extern struct ide_channel channels[];
void ide_read(struct disk* hd, uint32_t lba, void*buf, uint32_t sec_cnt);
void ide_write(struct disk* hd, uint32_t lba, void*buf, uint32_t sec_cnt);
#endif