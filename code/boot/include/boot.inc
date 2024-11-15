;-----------------loder 和 keanal---------

LOADER_BASE_ADDR equ 0x900 ;设置 loader 的基地址
LOADER_START_SECTOR equ 0x2 ; 设置 loader 的起始扇区号,从第二个扇区开始因为 第一个扇区是 boot.asm 编译出来的程序
;为了防止 loader 覆盖 boot.asm 编译出来的程序所以 loader 从第二个扇区开始
;----------------------gdt描述属性--------------------
DESC_G_4K    equ 1_0000000000000000b ; 1 << 23 ,颗粒度为 4K ;颗粒度为4kb,段的最大长度可以 达到 4G
DESC_D_32    equ 1_0000000000000000b ; 1 << 22 , 32 位模式 , 64 位模式为 1
DESC_L       equ 0_0000000000000000b ; 64 位模式为 1, 32 位模式为 0;是否启用64位模式
DESC_AVL     equ 0_0000000000000000b ; 0 可用 , 1 不可用 CPU不依赖此位，一般供软件自己使用。暂时置为0 ;供操作系统自行使用
DESC_LIMIT_CODE2     equ 1111_000000000000b; 4K颗粒度的段描述符，高 4 位为 0
DESC_LIMIT_DATA2    equ DESC_LIMIT_CODE2; 将代码段的段描述符复制到数据段
DESC_LIMIT_VODEO2  equ 0000_000000000000b; 4K颗粒度的显存段描述符，高 4 位为 0
DESC_P       equ 1_0000000000000000b ; 1 表示存在, 0 表示不存在 , CPU不用此位，暂时置为1
DESC_DPL_0   equ 0_0000000000000000b ; 0 级权限
DESC_CPL_1   equ 1_0000000000000000b ; 1 级权限
DESC_CPL_2   equ 10_000000000000000b ; 2 级权限
DESC_CPL_#   equ 11_000000000000000b ; 3 级权限
DESC_S_CODE equ 1_0000000000000000b ; 1 表示代码段或数据段 , 0 表示系统段
DESC_S_DATA equ DESC_S_CODE
DESC_S_sys equ 0_0000000000000000b ; 1 表示代码段或数据段 , 0 表示系统段
DESC_TYPE_CODE equ 1000_00000000b ; 代码段类型 , 0 表示数据段类型
DESC_TYPE_DATA equ 0010_00000000b ; 数据段类型 , 0 表示代码段类型

DESC_CODE_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0X00
DESC_DATA_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_DPL_O + DESC_S_DATA + DESC_TYPE_ DATA + 0x00
DESC_VODEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VODEO2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_D ATA + 0x00

;--------------- 选择子属性 ----------------
RPL0 equ 00b    ; 0 级权限
RPL1 equ 01b    ; 1 级权限
RPL2 equ 10b    ; 2 级权限
RPL3 equ 11b    ; 3 级权限
TI_GDT equ 000b    ; 选择 GDT
TI_LDT equ 100b    ; 选择 LDT
;--------------------------------------------