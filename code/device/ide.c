#include "ide.h"
#include "../thread/sync.h"
#include "io.h"
#include "stdio.h"
#include "stdio_kernel.h"
#include "interrupt.h"
#include "memory.h"
#include "debug.h"
#include "string.h"
#include "console.h"
#include "timer.h"
#include "list.h"

//定义硬盘各寄存器的端口号,两个通道不同寄存器的端口
#define reg_data(channel)           (channel->port_base + 0)    //数据寄存器，用于读写数据
#define reg_error(channel)          (channel->port_base + 1)    //错误寄存器
#define reg_sect_cnt(channel)       (channel->port_base + 2)    //扇区数寄存器
#define reg_lba_l(channel)          (channel->port_base + 3)    //LBA地址寄存器0-7位
#define reg_lba_m(channel)          (channel->port_base + 4)    //LBA地址寄存器8-15位
#define reg_lba_h(channel)          (channel->port_base + 5)    //LBA地址寄存器16-23位
#define reg_dev(channel)            (channel->port_base + 6)    //设备/命令寄存器
#define reg_status(channel)         (channel->port_base + 7)    //状态寄存器
#define reg_cmd(channel)            (reg_status(channel))       //命令寄存器
#define reg_alt_status(channel)     (channel->port_base + 0x206)    //备用状态寄存器
#define reg_ctl(channel)            reg_alt_status(channel)

//reg_alt_status寄存器的一些关键位
#define BIT_STAT_BSY        0x80        //硬盘忙
#define BIT_STAT_DRDY       0x40        //驱动器准备好
#define BIT_STAT_DRQ        0x08        //数据传输准备好了

//device寄存器的一些关键位
#define BIT_DEV_MBS         0xa0        //第7位和第5位固定为1
#define BIT_DEV_LBA         0x40        //LBA模式
#define BIT_DEV_DEV         0x10        //主从设备位，1表示从设备

//一些硬盘操作的指令
#define CMD_IDENTIFY        0xec        //identify指令
#define CMD_READ_SECTOR     0x20        //读扇区指令
#define CMD_WRITE_SECTOR    0x30        //写扇区指令

//定义可读写的最大扇区数，调试使用
#define max_lba             ((80*1024*1024/512)-1)

uint8_t channel_cnt;                    //按硬盘数计算的通道数
struct ide_channel channels[2];         //有两个ide通道

//用于记录总拓展分区的起始lba,初始为0,partition_scan时依次为标记
int32_t ext_lba_base = 0;

//用来记录硬盘主分区和逻辑分区的下标
uint8_t p_no = 0, l_no = 0;

//分区队列
struct list partition_list;

//构建一个16字节大小的结构体，用来存分区表项
struct partition_table_entry {
    uint8_t bootable;            //是否可引导
    uint8_t start_head;          //起始磁头号
    uint8_t start_sec;           //起始扇区号
    uint8_t start_chs;           //起始柱面号
    uint8_t fs_type;             //分区类型
    uint8_t end_head;            //结束磁头号
    uint8_t end_sec;             //结束扇区号
    uint8_t end_chs;             //结束柱面号
    //更需要关注的是下面这两项
    uint32_t start_lba;          //本分区起始扇区的lba地址
    uint32_t sec_cnt;            //本分区的扇区数
}__attribute__ ((packed));	     // 保证此结构是16字节大小

//引导扇区，mbr或ebr所在的扇区
struct boot_sector {
    uint8_t other[446];            //引导代码
    struct partition_table_entry partition_table[4];    //分区表4项，共64字节
    uint16_t signature;            //MBR的结束标志0x55,0xaa
}__attribute__ ((packed));

//选择读写的硬盘
static void select_disk(struct disk* hd) {
    //设置设备寄存器的值
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA ;
    if(hd->dev_no == 1) {        //若是从盘，设置DEV为1
        reg_device |= BIT_DEV_DEV;
    }
    outb(reg_dev(hd->my_channel), reg_device);
}

//向硬盘控制器写入起始扇区地址及要读写的扇区数
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {
    ASSERT(lba <= max_lba);
    struct ide_channel* channel = hd->my_channel;

    //写入要读取的扇区数
    outb(reg_sect_cnt(channel), sec_cnt);        //如果sec_cnt为0表示写入256个扇区

    //写入lba地址
    outb(reg_lba_l(channel), lba);                //lba地址的低8位，不再单独取出，outb函数中的汇编指令outb %b0, %w1会只用al
    outb(reg_lba_m(channel), lba >> 8);           //lba地址的8~15位
    outb(reg_lba_h(channel), lba >> 16);          //lba地址的16~23位

    //因为lba地址的24~27位要存储在device寄存器的0~3位，所以要单独取出
    //无法单独写入这4位，这里把device寄存器在重新写入一次
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

//向通道channel发送命令cmd
static void cmd_out(struct ide_channel* channel, uint8_t cmd) {
    //只要向硬盘发出了命令便将此标记置为true,硬盘中断处理程序需要根据它来判断
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}

//硬盘读入sec_cnt个扇区的数据到buf中
static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
    uint32_t size_in_byte;
    if(sec_cnt == 0) {
        //因为sec_cnt是8位变量，由主调函数将其赋值时，若为256会将最高位1丢掉变为0
        size_in_byte = 256 * 512;
    }
    else {
        size_in_byte = sec_cnt * 512;
    }
    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

//将buf中sec_cnt扇区的数据写入硬盘
static void write2sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
    uint32_t size_in_byte;
    if(sec_cnt == 0) {
        size_in_byte = 256 * 512;
    }
    else {
        size_in_byte = sec_cnt * 512;
    }
    outsw(reg_data(hd->my_channel),buf,size_in_byte/2);
}

//等待30s
static bool busy_wait(struct disk* hd) {
    struct ide_channel* channel = hd->my_channel;
    uint16_t time_limit = 30 * 1000;        //可以等待30000ms
    while(time_limit -= 10 >= 0) {
        if(!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
            return (inb(reg_status(channel)) & BIT_STAT_DRQ);       //DRQ表示硬盘准备好数据传输
        }
        else {
            mtime_sleep(10);                    //等待10ms
        }
    }
    return false;
}

//从硬盘读取sec_cnt个扇区到buf
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);

    //1.先选择要操作的硬盘
    select_disk(hd);

    uint32_t secs_op;            //每次操作的扇区数
    uint32_t secs_done = 0;          //已经完成的扇区数
    while(secs_done < sec_cnt) {
        if((secs_done + 256) <= sec_cnt) {
            secs_op = 256;
        }
        else {
            secs_op = sec_cnt - secs_done;
        }

        //2.写入待读入的扇区数和起始扇区号
        select_sector(hd, lba + secs_done, secs_op);

        //3.执行的命令写入reg_cmd寄存器
        cmd_out(hd->my_channel, CMD_READ_SECTOR);        //准备开始读取数据

        /*********************   阻塞自己的时机  ***********************
          在硬盘已经开始工作(开始在内部读数据或写数据)后才能阻塞自己,现在硬盘已经开始忙了,
          将自己阻塞,等待硬盘完成读操作后通过中断处理程序唤醒自己*/
        sema_down(&hd->my_channel->disk_done);
        /*************************************************************/

        //4.检测硬盘状态是否可读
            //醒来后执行
        if(!busy_wait(hd)) {
            char error[64];
            sprintf(error,"%s read sector %d failed!!!!!\n", hd->name, lba);
            PANIC(error);
        }

        //5.把数据从硬盘的缓冲区中读出
        read_from_sector(hd, (void* )((uint32_t)buf + secs_done * 512), secs_op);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}

//将buf中sec_cnt扇区数据写入硬盘
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
    ASSERT(lba <= max_lba && sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);

    //1.先选择要操作的硬盘
    select_disk(hd);

    uint32_t secs_op;           //每次操作的扇区数
    uint32_t secs_done = 0;     //已经完成的扇区数

    //确定每次操作的扇区数大小
    while(secs_done < sec_cnt) {
        if((secs_done + 256) <= sec_cnt) {
            secs_op = 256;
        }
        else {
            secs_op = sec_cnt - secs_done;
        }

        //2.写入待写入的扇区数和起始扇区号
        select_sector(hd, lba + secs_done, secs_op);

        //3.执行的命令写入reg_cmd寄存器
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);    //准备开始写入数据

        //4.检测硬盘状态是否可写
        if(!busy_wait(hd)) {
            char error[64];
            sprintf(error, "%s write sector %d failed!!!!!!\n", hd->name, lba);
            PANIC(error);
        }

        //5.将数据写入硬盘
        write2sector(hd,(void* )(uint32_t)buf + secs_done * 512, secs_op);

        //在硬盘响应期间阻塞自己
        sema_down(&hd->my_channel->disk_done);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}

//硬盘中断处理程序
void intr_hd_handler(uint8_t irq_no) {
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);
    uint8_t ch_no = irq_no - 0x2e;
    struct ide_channel* channel = &channels[ch_no];
    ASSERT(channel->irq_no == irq_no);
    /* 不必担心此中断是否对应的是这一次的expecting_intr,
     * 每次读写硬盘时会申请锁,从而保证了同步一致性 */
    if (channel->expecting_intr) {
        channel->expecting_intr = false;
        sema_up(&channel->disk_done);

        /* 读取状态寄存器使硬盘控制器认为此次的中断已被处理,从而硬盘可以继续执行新的读写 */
        inb(reg_status(channel));
    }
}

//将dst中len个相邻字节交换位置后存入buf中
static void swap_pairs_bytes(const char* dst, char* buf, uint32_t len) {
    uint8_t index;
    for(index = 0; index < len ; index += 2) {
        buf[index + 1] = *dst++;
        buf[index] = *dst++;
    }
    buf[index] = '\0';
}

//获得硬盘参数信息
static void identify_disk(struct disk* hd) {
    char id_info[512];
    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTIFY);
    /* 向硬盘发送指令后便通过信号量阻塞自己,
     * 待硬盘处理完成后,通过中断处理程序将自己唤醒 */
    sema_down(&hd->my_channel->disk_done);

    /* 醒来后开始执行下面代码*/
    if (!busy_wait(hd)) {     //  若失败
        char error[64];
        sprintf(error, "%s identify failed!!!!!!\n", hd->name);
        PANIC(error);
    }
    read_from_sector(hd, id_info, 1);

    char buf[64];
    uint8_t sn_start = 10 * 2,sn_len = 20, md_start = 27 * 2, md_len =40;
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
    printk("   disk %s info:\n      SN: %s\n", hd->name, buf);
    memset(buf, 0, sizeof(buf));
    swap_pairs_bytes(&id_info[md_start], buf, md_len);
    printk("      MODULE: %s\n", buf);
    uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
    printk("      SECTORS: %d\n", sectors);
    printk("      CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}

//扫描硬盘hd中地址为ext_lba的扇区中的所有分区
static void partition_scan(struct disk* hd, uint32_t ext_lba) {
    struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
    ide_read(hd,ext_lba,bs,1);
    uint8_t part_idx = 0;
    struct partition_table_entry* p = bs->partition_table;

    //便利分区表4个分区表项
    while(part_idx++ < 4) {
        if(p->fs_type == 0x5) { //若为拓展分区(==)********************************
            if(ext_lba_base != 0) {
                //子拓展分区的start_lba是相对于主引导扇区中的总拓展分区地址
                partition_scan(hd, p->start_lba + ext_lba_base);
            }
            else {    // ext_lba_base为0表示是第一次读取引导块,也就是主引导记录所在的扇区
                /* 记录下扩展分区的起始lba地址,后面所有的扩展分区地址都相对于此 */
                ext_lba_base = p->start_lba;
                partition_scan(hd, p->start_lba);
            }
        }
        else if(p->fs_type != 0) {        //若是有效的分区类型
            if(ext_lba == 0) {        //此时全是主分区
                hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
                hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
                hd->prim_parts[p_no].my_disk = hd;
                list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
                sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
                p_no++;
                ASSERT(p_no < 4);	    // 0,1,2,3
            }
            else {
                hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
                hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
                hd->logic_parts[l_no].my_disk = hd;
                list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
                sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name, l_no + 5);	 // 逻辑分区数字是从5开始,主分区是1～4.
                l_no++;
                if (l_no >= 8)    // 只支持8个逻辑分区,避免数组越界
                    return;
            }
        }
        p++;
    }
    sys_free(bs);
}

//打印分区信息
static bool partition_info(struct list_elem* pelem, int arg UNUSED) {
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    printk("   %s start_lba:0x%x, sec_cnt:0x%x\n",part->name, part->start_lba, part->sec_cnt);

    /* 在此处return false与函数本身功能无关,
     * 只是为了让主调函数list_traversal继续向下遍历元素 */
    return false;
}

//硬盘数据结构初始化
void ide_init() {
    printk("ide_init start\n");
    uint8_t hd_cnt = *((uint8_t*)(0x475));    //获取硬盘数量
    printk("    ide_init hd_cnt:%d\n",hd_cnt);
    ASSERT(hd_cnt > 0);
    list_init(&partition_list);
    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);    //一个ide通道可以连接两个硬盘，根据硬盘数反推
    struct ide_channel* channel;
    uint8_t channel_no = 0;
    uint8_t dev_no = 0;

    //处理每个通道上的硬盘
    while(channel_no < channel_cnt) {
        channel = &channels[channel_no];
        sprintf(channel->name, "ide%d", channel_no);

        //为每个ide通道初始化端口基址及中断向量
        switch(channel_no) {
            case 0:
                channel->port_base = 0x1f0;        //ide0通道的起始端口号是0x1f0
                channel->irq_no = 0x20 + 14;       //从片8259A上的倒数第二中断引脚，来相应ide0通道的中断向量号
                break;
            case 1:
                channel->port_base = 0x170;        //ide1通道的起始端口号是0x170
                channel->irq_no = 0x20 + 15;        //从片8259A上的最后一个中断引脚，来相应ide1通道的中断向量号
                break;
        }

        channel->expecting_intr = false;            //未向硬盘写入指令时不期待硬盘的中断
        lock_init(&channel->lock);                     //初始化锁

        //初始化为0,目的是向硬盘控制器请求数据后，硬盘驱动sema_down此信号量会阻塞线程
        //知道硬盘完成后通过发中断，中断处理程序将此信号量sema_up,唤醒线程
        sema_init(&channel->disk_done, 0);
        register_handler(channel->irq_no, intr_hd_handler);

        while(dev_no < 2) {
            struct disk* hd = &channel->devices[dev_no];
            hd->my_channel = channel;
            hd->dev_no = dev_no;
            sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
            identify_disk(hd);            //向硬盘写入指令，获取硬盘信息
            if(dev_no !=0) {
                partition_scan(hd,0);        //扫描硬盘的分区表
            }
            p_no = 0; l_no = 0;
            dev_no++;
        }
        dev_no = 0;
        channel_no++;            //处理下一个通道
    }
    printk("\n   all partition info\n");
    /* 打印所有分区信息 */
    list_traversal(&partition_list, partition_info, (int)NULL);
    printk("ide_init done\n");
}