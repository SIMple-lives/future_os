;-----------------loder 和 keanal---------

LOADER_BASE_ADDR equ 0x900 ;设置 loader 的基地址
LOADER_START_SECTOR equ 0x2 ; 设置 loader 的起始扇区号,从第二个扇区开始因为 第一个扇区是 boot.asm 编译出来的程序
;为了防止 loader 覆盖 boot.asm 编译出来的程序所以 loader 从第二个扇区开始


KERNEL_START_SECTOR equ 0x9     ;从第九扇区开始加载内核
KERNEL_BIN_BASE_ADDR equ 0x70000    ; 内核的虚拟地址
KERNEL_ENTER_ADDR equ 0xc0001500    ; 内核入口地址
PT_NULL equ 0x0

PAGE_DIR_TABLE_POS equ 0x100000 ; 页目录表的位置，出了低端1MB的第一字节.

;----------------------gdt描述属性--------------------
DESC_G_4K    equ 1_00000000000000000000000b ; 1 << 23 ,颗粒度为 4K ;颗粒度为4kb,段的最大长度可以 达到 4G
DESC_D_32    equ  1_0000000000000000000000b ; 1 << 22 , 32 位模式 , 64 位模式为 1
DESC_L       equ   0_000000000000000000000b ; 64 位模式为 1, 32 位模式为 0;是否启用64位模式
DESC_AVL     equ    0_00000000000000000000b ; 0 可用 , 1 不可用 CPU不依赖此位，一般供软件自己使用。暂时置为0 ;供操作系统自行使用


DESC_LIMIT_CODE2     equ 1111_0000000000000000b; 4K颗粒度的段描述符，高 4 位为 0
DESC_LIMIT_DATA2    equ DESC_LIMIT_CODE2; 将代码段的段描述符复制到数据段
DESC_LIMIT_VIDEO2  equ 0000_0000000000000000b; 4K颗粒度的显存段描述符，高 4 位为 0


DESC_P       equ 1_000000000000000b ; 1 表示存在, 0 表示不存在 , CPU不用此位，暂时置为1
DESC_DPL_0   equ 00_0000000000000b ; 0 级权限
DESC_CPL_1   equ 01_0000000000000b ; 1 级权限
DESC_CPL_2   equ 10_0000000000000b ; 2 级权限
DESC_CPL_#   equ 11_0000000000000b ; 3 级权限
DESC_S_CODE equ 1_000000000000b ; 1 表示代码段或数据段 , 0 表示系统段
DESC_S_DATA equ DESC_S_CODE
DESC_S_sys equ 0_000000000000b ; 1 表示代码段或数据段 , 0 表示系统段
DESC_TYPE_CODE equ 1000_00000000b ; 代码段类型 , 0 表示数据段类型
DESC_TYPE_DATA equ 0010_00000000b ; 数据段类型 , 0 表示代码段类型

DESC_CODE_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0x00
DESC_DATA_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00
DESC_VIDEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0B


;--------------- 选择子属性 ----------------
RPL0 equ 00b    ; 0 级权限
RPL1 equ 01b    ; 1 级权限
RPL2 equ 10b    ; 2 级权限
RPL3 equ 11b    ; 3 级权限
TI_GDT equ 000b    ; 选择 GDT
TI_LDT equ 100b    ; 选择 LDT
;--------------------------------------------

;-------- 页表相关属性 ----------
PG_P equ 1b ; 页表项有效
PG_RW_R equ 00b ; 页表项可读
PG_RW_W equ 10b ; 页表项可写
PG_US_U equ 100b ; 系统模式
PG_US_S equ 000b ; 用户模式