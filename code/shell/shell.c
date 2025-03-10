#include "shell.h"
#include "stdint.h"
#include "fs.h"
#include "file.h"
#include "stdio.h"
#include "global.h"
#include "assert.h"
#include "string.h"
#include "syscall.h"
#include "buildin_cmd.h"

#define cmd_len 128            //最大支持键入128个字符命令行输入
#define MAX_ARG_NR 16          //加上命令名外，最多支持15个参数

//存入输入的命令
static char cmd_line[MAX_PATH_LEN] = {0};
char final_path[MAX_PATH_LEN] = {0};        //用于洗路径时的缓冲

//用来记录当前目录,是当前目录的缓存,每次执行cd命令时会更新此内容
char cwd_cache[MAX_PATH_LEN] = {0};

//输出提示符
void print_prompt(void) {
    printf("[rabbit@localhost %s]$ ", cwd_cache);
}

//从键盘缓冲区中最多读入count个字节到buf
static void readline(char* buf, int32_t count) {
    assert(buf != NULL && count > 0);
    char* pos = buf;
    while(read(stdin_no, pos, 1) != -1 && (pos - buf) < count) {        //在不出错的情况下，直到找到回车符才返回
        switch (*pos) {
            /* 找到回车或换行符后认为键入的命令结束,直接返回 */
        case '\n':
        case '\r':
           *pos = 0;	   // 添加cmd_line的终止字符0
            putchar('\n');
            return;

        case '\b':
            if (buf[0] != '\b') {		// 阻止删除非本次输入的信息
                --pos;	   // 退回到缓冲区cmd_line中上一个字符
                putchar('\b');
            }
            break;

            /* ctrl+l 清屏 */
        case 'l' - 'a':
            /* 1 先将当前的字符'l'-'a'置为0 */
                *pos = 0;
            /* 2 再将屏幕清空 */
            clear();
            /* 3 打印提示符 */
            print_prompt();
            /* 4 将之前键入的内容再次打印 */
            printf("%s", buf);
            break;

            /* ctrl+u 清掉输入 */
        case 'u' - 'a':
            while (buf != pos) {
                putchar('\b');
                *(pos--) = 0;
            }
            break;

            /* 非控制键则输出字符 */
        default:
            putchar(*pos);
            pos++;
        }
    }
    printf("readline: can`t find enter_key in the cmd_line, max num of char is 128\n");
}

//分析字符串cmd_str中以token为分隔符的单词，将单词存入argv数组
static int32_t cmd_parse(char* cmd, char** argv, char token)
{
    assert(cmd != NULL);
    int32_t arg_idx = 0;
    while (arg_idx < MAX_ARG_NR)
    {
        argv[arg_idx++] = NULL;
    }
    char* next = cmd;
    int32_t argc = 0;
    //外层循环处理整个命令行
    while (*next)
    {
        //去除命令字或参数之间的空格
        while (*next == token)
        {
            next++;
        }
        //处理最后一个参数后接空格的情况
        if (*next == 0)
        {
            break;
        }
        argv[argc] = next;

        //内层循环处理命令行中的每个命令字及参数
        while (*next && *next != token)     //在字符串结束前找单词分割符
        {
            next++;
        }

        //如果未结束(是token字符),使token变为0
        if (*next)
        {
            *next++ = 0;
        }

        //避免argv数组越界，参数过多返回0
        if (argc > MAX_ARG_NR)
        {
            return -1;
        }
        argc++;
    }
    return argc;
}

char* argv[MAX_ARG_NR];         //argv必须为全局变量，为了以后exec的程序可访问参数
int32_t argc = -1;

//简单的shell
void my_shell(void) {
    cwd_cache[0] = '/';
    while (1) {
        print_prompt(); 
        memset(final_path, 0, MAX_PATH_LEN);
        memset(cmd_line, 0, MAX_PATH_LEN);
        readline(cmd_line, MAX_PATH_LEN);
        if (cmd_line[0] == 0) {	 // 若只键入了一个回车
	        continue;
        }

        argc = -1;
        argc = cmd_parse(cmd_line, argv, ' ');
        if (argc == -1) {
	        printf("num of arguments exceed %d\n", MAX_ARG_NR);
	        continue;
        }
        if (!strcmp("ls", argv[0])) {
	        buildin_ls(argc, argv);
        } 
        else if (!strcmp("cd", argv[0])) {
	        if (buildin_cd(argc, argv) != NULL) {
	            memset(cwd_cache, 0, MAX_PATH_LEN);
	            strcpy(cwd_cache, final_path);
	        }
        } 
        else if (!strcmp("pwd", argv[0])) {
	        buildin_pwd(argc, argv);
        }  
        else if (!strcmp("ps", argv[0])) {
	        buildin_ps(argc, argv);
        } 
        else if (!strcmp("clear", argv[0])) {
	        buildin_clear(argc, argv);
        } 
        else if (!strcmp("mkdir", argv[0])){
	        buildin_mkdir(argc, argv);
        } 
        else if (!strcmp("rmdir", argv[0])){
	        buildin_rmdir(argc, argv);
        } 
        else if (!strcmp("rm", argv[0])) {
	        buildin_rm(argc, argv);
        } 
        else {      // 如果是外部命令,需要从磁盘上加载
	        int32_t pid = fork();
	        if (pid) {	   // 父进程
	            int32_t status;
	            int32_t child_pid = wait(&status);          // 此时子进程若没有执行exit,my_shell会被阻塞,不再响应键入的命令
	            if (child_pid == -1) {     // 按理说程序正确的话不会执行到这句,fork出的进程便是shell子进程
	                panic("my_shell: no child\n");
	            }
	        printf("child_pid %d, it's status: %d\n", child_pid, status);
	        } 
            else {	   // 子进程
	            make_clear_abs_path(argv[0], final_path);
	            argv[0] = final_path;
	            /* 先判断下文件是否存在 */
	            struct stat file_stat;
	            memset(&file_stat, 0, sizeof(struct stat));
	            if (stat(argv[0], &file_stat) == -1) {
	                printf("my_shell: cannot access %s: No such file or directory\n", argv[0]);
	                exit(-1);
	            } 
                else {
	                execv(argv[0], argv);
	            }
	        }
        }
        int32_t arg_idx = 0;
        while(arg_idx < MAX_ARG_NR) {
	        argv[arg_idx] = NULL;
	        arg_idx++;
        }
    }
   panic("my_shell: should not be here");
}

// void my_shell(void) {
//     cwd_cache[0] = '/';
//     cwd_cache[1] = 0;
//     while(1) {
//         print_prompt();
//         memset(cmd_line, 0, MAX_PATH_LEN);
//         memset(final_path, 0, MAX_PATH_LEN);
//         readline(cmd_line, MAX_PATH_LEN);
//         if (cmd_line[0] == 0) {   //如果用户没有输入任何内容，直接回车，则继续
//             continue;
//         }
//         argc = -1;
//         argc = cmd_parse(cmd_line, argv, ' ');
//         if (argc == -1)
//         {
//             printf("num of arguments exceed %d\n", MAX_ARG_NR);
//             continue;
//         }
//
//         char buf[MAX_PATH_LEN];
//         memset(buf, 0, MAX_PATH_LEN);
//         int32_t arg_idx = 0;
//         while (arg_idx < argc)
//         {
//             make_clear_abs_path(argv[arg_idx], buf);
//             printf("%s -> %s\n", argv[arg_idx], buf);
//             // printf("%s ", argv[arg_idx]);
//             arg_idx++;
//         }
//         // printf("\n");
//     }
//     panic("my_shell: should not be here");
// }