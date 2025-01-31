#ifndef __FS_FS_H
#define __FS_FS_H
#include "stdint.h"
// #include "ide.h"

#define MAX_FILES_PER_PART        4096    //每个分区所支持的最大文件数
#define BITS_PER_SECTOR           4096    //每扇区的位数
#define SECTOR_SIZE               512     //每个扇区的大小    字节
#define BLOCK_SIZE                512     //块字节大小(SECTOR_SIZE)

#define MAX_PATH_LEN              512    //文件路径的最大长度

//文件类型
enum file_types {
    FT_UNKNOWN,    //不支持的文件类型
    FT_REGULAR,    //普通文件
    FT_DIRECTORY,  //目录文件
};

//打开文件的选项
enum oflags {
    O_RDONLY,      //只读
    O_WRONLY,      //只写
    O_RDWR,        //读写
    O_CREAT = 4    //如果文件不存在，则创建
};

//文件读写位置偏移量
enum whence {
    SEEK_SET = 1, //文件开头
    SEEK_CUR,     //当前位置
    SEEK_END      //文件结尾
};

//用来记录查找文件过程中已经找到的上级路径，也就是文件查找过程中"走过的地方“
struct path_search_record {
    char searched_path[MAX_PATH_LEN];    //已经查找过的父路径
    struct dir* parent_dir;				 //文件或目录所在的直接父目录
    enum file_types file_type;			 //找到的是普通文件还是目录。找不到将为FT_UNKNOWN
};

//文件属性结构体
struct stat {
    uint32_t st_ino;        // inode编号
    uint32_t st_size;       // 文件大小
    enum file_types st_filetype; // 文件类型
};

extern struct partition* cur_part;
void filesys_init(void);
char* path_parse(char* pathname, char* name_store);
int32_t path_depth_cnt(char* pathname);
int32_t sys_open(const char* pathname, uint8_t flags);
int32_t sys_close(int32_t fd);
int32_t sys_write(int32_t fd, const void* buf, uint32_t count);
int32_t sys_read(int32_t fd, void* buf, uint32_t count);
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t sys_unlink(const char* pathname);
int32_t sys_mkdir(const char* pathname);
struct dir* sys_opendir(const char* pathname);
int32_t sys_closedir(struct dir* dir);
struct dir_entry* sys_readdir(struct dir* dir);
void sys_rewinddir(struct dir* dir);
int32_t sys_rmdir(const char* pathname);
char* sys_getcwd(char* buf, uint32_t size);
int32_t sys_chdir(const char* path);
int32_t sys_stat(const char* path, struct stat* buf);
void sys_putchar(char char_asci);
uint32_t fd_local2global(uint32_t local_fd);
#endif

