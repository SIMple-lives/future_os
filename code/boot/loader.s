    %include "boot.inc"
    section loader vstart=LOADER_BASE_ADDR
    LOADER_STACK_TOP equ LOADER_BASE_ADDR   ; 用于在保护模式下的栈顶
    jmp loader_start

;构建gdt以及其内部的描述符
    GDT_BASE: dd 0x00000000   ;gdt的起始地址
                dd 0x00000000

    CODE_DESC: dd 0x0000FFFF        ;code段描述符的起始地址    代码段
                dd DESC_CODE_HIGH4

    DATA_STACK_DESC: dd 0x0000FFFF  ; 数据段和栈段描述符的起始地址
                dd DESC_DATA_HIGH4

VIDEO_DESC: dd 0x80000007;0x8000000;limit=(0xbffff-0xb8000)/4096=0x7ff     ;显存段描述符
                dd DESC_VIDEO_HIGH4 ; 此时dpl为0，表示用户级

    GDT_SIZE equ $ - GDT_BASE
    GDT_LIMIT equ GDT_SIZE - 1

    times 60 dq 0     ;此处预留60个描述符 ，防止后面扩展段时出错以便后面扩展

    SELECTOR_CODE equ (0x0001 << 3) + TI_GDT + RPL0 ;相当于(CODE_DESC - GDT_BASE) / 8 + TI_GDT + RPL0
    SELECTOR_DATA equ ( 0x0002 << 3) + TI_GDT + RPL0
    SELECTOR_VIDEO equ ( 0x0003 << 3) + TI_GDT + RPL0

    ; total_mem_bytes 用于保存内存容量大小，单位为字节
    ; 当前偏移 loader.bin 文件头的大小为0x200字节
    ; loader.bin的加载地址是0x900
    ; 故total_mem_bytes的地址为 0b00
    ; 后面会引用该地址
    total_mem_bytes dd 0

    ;以下是gdt的指针，前两字节是gdt界限，后4字节是gdt起始地址
    gdt_ptr dw GDT_LIMIT
            dd GDT_BASE

    ;人工对齐:total_mem_bytes4+gdt_ptr6+ards_buf244+ards_nr2 ，共256字节
    ;ards_buf用于保存ards结构体，ards_nr用于保存ards结构体个数，一个ards结构体大小为20字节，12个+4字节大小的缓冲空间
    adrs_buf times 244 db 0 ; db 定义字节类型
    ards_nr dw 0 ; dw 定义字类型
       ;loadermsg db '2 loader in real.'

       loader_start:

        ;int 15h eax = 0000E820h , edx = 534D4150h ('SMAP') 获取内存布局
        xor ebx ,ebx ;ebx的值为0, xor指令用于清零
        mov edx ,0x534d4150
        mov di , adrs_buf
    .e820_mem_get_loop:
        ;mov byte [gs:0], 'E'
        mov eax ,0x0000e820     ;执行int 0x15后，eax的值变为0x534d4150
;每次执行int前都需要更新为子功能号，否则会出错
        mov ecx ,20 ;ecx的值为20, 20字节
        int 0x15
        jc .e820_failed_so_try_e801
        ;mov byte [gs:2], 'S'
;若cf位为1,则表示有错误发生，尝试使用0xe801子功能
        add di , cx     ;使di增加20字节指向缓冲区中的新的ARDS结构位置
        inc word [ards_nr]  ;inc 指令用于增加ards_nr的值加一,记录ards的数量
        cmp ebx ,0        ;若ebx的值为0且cf不为1,则表示已经获取了所有的ARDS结构到了所有内存信息，退出循环
        jnz .e820_mem_get_loop

;在所有ards结构中，找出(base_add_low+length_low)的最大值，即内存的容量
        mov cx , [ards_nr] ;cx的值为ards_nr的值
;便利ARDS结构，找出最大值
        mov ebx ,adrs_buf
        xor edx ,edx        ;edx为最大的内存，在此刻先清0
.find_max_mem_area:         ;无需判断type是否为1,因为只有type为1的才是可用的内存最大的内存一定是可使用的
        mov eax ,[ebx]
        add eax ,[ebx+8]
        add ebx ,20
        cmp edx ,eax
;冒泡排序，找出最大的值，edx寄存器始终是最大的值
        jge .next_ards
        mov edx , eax
.next_ards:
        loop .find_max_mem_area
        jmp .mem_get_ok

;-------- int 15h ax = E801h 获取内存容量，最大支持4GB ---------------
.e820_failed_so_try_e801:
        mov byte [gs:0], 'D'
        mov eax ,0xe801
        int 0x15
        jc .e801_failed_so_try88    ;若当前e801函数失败，就尝试0x88子功能

;1 先计算出低15MB的内存     ;ax和cx当中的单位为KB,先将其转换为字节 1kb=1024字节
        mov cx ,0x400
        mul cx
        shl edx ,16     ;shl 左移16位，将edx中的值左移16位，即edx中的值乘以65536
        and eax ,0x0000FFFF     ;and 与运算，将eax中的值与0x0000FFFF进行与运算，即取eax中的低16位
        or edx ,eax     ;or 或运算，将edx和eax中的值进行或运算，即edx和eax中的值相加
        add edx ,0x100000    ;加上1mb的大小
        mov esi ,edx    ;esi寄存器保存低15MB的内存大小

;2 再将16MB以上的内存转换为byte为单位， 寄存器bx和dx中是以64KB为单位的内存数量
        xor eax ,eax
        mov ax ,bx
        mov ecx ,0x10000    ;十进制为64kb
        mul ecx         ;32位乘法，默认的被乘数是eax,积为64位    ;高32为存储在edx中，低32位存储在eax中
        add esi ,eax    ;只能测出4GB以内的内存，所以32为eax足够了 ;edx为0,只加eax中的值
        mov edx ,esi
        jmp .mem_get_ok

;-------- int 15h ah = 0x88 获取内存的大小，只能获取64MB之内的内存 -----------------
.e801_failed_so_try88:
        ;int 15后，ax存入的是以KB为单位的内存大小
        mov ah ,88
        int 0x15
        jc .error_hlt
        and eax ,0x0000FFFF

        ;16位乘法，被乘数是ax,积为32位，高16位存储在dx中，低16位存储在ax中
        mov cx ,0x400   ;0x400等于1024,将ax中的内存容量转换为一字节为单位
        mul cx
        shl edx ,16             ;把dx移动到高16位
        or edx ,eax             ;把积的低16位和edx合并，为32位的积
        add edx ,0x100000       ;0x88子功能只会返回1MB以上的内存，所以加上1MB

.mem_get_ok:
        mov [total_mem_bytes] ,edx





;----------------------------------------
;INT 0x10    功能号:0x13   功能描述：打印字符串
;----------------------------------------
;输入：
;AH 子功能号=13H
;BH = 页码
;BL = 属性(颜色)(若AL=00H或01H)
;CX = 字符串长度
;(DH,DL)=坐标(行、列)
;ES:BP = 字符串首地址
;AL = 显示输出方式(00H-80x25字符模式，01H-80x25文本模式，02H-320x200色图模式，03H-640x200色图模式，04H-320x200文本模式，05H-320x200色图模式，06H-640x350色图模式，07H-640x350文本模式，08H-720x400色图)
; 0------字符串中只含显示字符，其显示属性由BL决定;显示后光标不会移动;
; 1------字符串中只含显示字符，其显示属性由BL决定;显示后光标会移动到下一行;
; 2------字符串中只含显示字符，其显示属性由BL决定;显示后光标不会移动;
; 3------字符串中只含显示字符，其显示属性由BL决定;显示后光标会移动到下一行;
; 4------字符串中只含显示字符，其显示属性由BL决定;显示后光标不会移动;
; 5------字符串中只含显示字符，其显示属性由BL决定;
;无返回值
    ; mov sp ,LOADER_BASE_ADDR
    ; mov bp ,loadermsg ;
    ; mov cx ,17 ;cx=字符串长度
    ; mov ax ,0x1301 ;AH=13H,AL=01h
    ; mov bx ,0x001f ;页号为0(BH = 0) 蓝底粉红字(BL = 1fh)
    ; mov dx ,0x1800     ;这里决定输出到第21行，列号为0。也就是最底下一行的地方
    ; int 0x10 ;调用BIOS

;------------------进入保护模式-------------------
;1.打开A20地址的A20端口线
;2.设置GDT表描述符，设置GDTR
;3.将cr0的PE位置1，进入保护模式
    ;----------------------- 打开A20地址的A20端口线----------------------
    in al ,0x92     ;   读取A20端口
    or al ,0000_0010B ; 将A20端口线置1设置A20
    out 0x92 ,al    ;   写回A20端口

    ;----------------------- 加载GDT表描述符，设置GDTR----------------------
    lgdt [gdt_ptr] ; 加载GDT表描述符，设置GDTR


    ;----------------------- 将cr0的PE位置1，进入保护模式----------------------
    mov eax, cr0
    or eax, 0x00000001 ; 设置PE位
    mov cr0, eax ; 进入保护模式

    jmp SELECTOR_CODE:p_mode_start   ; 跳转到保护模式代码段 刷新流水线

    .error_hlt:        ;出错则挂起
        hlt

[bits 32]
p_mode_start:
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP
    mov ax,SELECTOR_VIDEO
    mov gs,ax

    ;mov byte [gs:160], 'P'

    ;jmp $

; 创建页目录表和页表并初始化内存位图
call setup_page

;要将描述符地址以及偏移量写入内存 gdt_ptr ，一会用新的地址重新加载
sgdt [gdt_ptr]  ;存储到原来的gdt_ptr中

;将gdt描述符中视频段描述符中的段基址+0xc0000000
mov ebx , [gdt_ptr + 2]
or dword [ebx + 0x18 + 4] , 0xc0000000
;视频段是第三个段描述符，每个描述符大小是8字节，故偏移量是0x18
;段描述符的高4字节的最高位是段基址的第31~24位

;将gdt的基址加上0xc0000000 使其成为内核所在的高地址
add dword [gdt_ptr + 2] , 0xc0000000

add esp , 0xc0000000    ;将栈指针同样映射到内核地址

; 把页目录地址赋给cr3
mov eax , PAGE_DIR_TABLE_POS
mov cr3 , eax

; 打开cr0的PG位(第31位)，开启分页
mov eax , cr0
or eax , 0x80000000
mov cr0 , eax

; 在开启分页后，用gdt新的地址重新加载
lgdt [gdt_ptr]      ;重新加载

mov byte [gs:160], 'G'

jmp $

;--------------------- 创建页目录及页页表 ----------------------
setup_page:
;先将页目录占用的空间逐字节清0
    mov ecx , 4096
    mov esi , 0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

;开始创建页目录项(PDE)
.create_pde:
    mov eax , PAGE_DIR_TABLE_POS
    add eax , 0x1000    ;此时eax为第一个页表 的物理地址和属性
    mov ebx , eax   ;此处位ebx赋值，为后面.create_pte做准备

;   下面将页目录项0 和 0xc00 都存为第一个页表的地址 ，每一个页表表示4MB内存
;   这样0xc03fffff 以下的地址 和 0x003fffff 以下的地址 都指向同一个页表
;   这是为将地址映射为内核地址做准备

    or eax , PG_US_U | PG_RW_W | PG_P
;   页目录项的属性RW和P为1，表示可读可写，US为1, 所有特权级都可以访问

    mov [PAGE_DIR_TABLE_POS + 0x0] , eax ;   将第一个页表地址存入页目录项0
;   在页目录表中的第一个目录写入一个页表的位置(0x101000)以及属性(7)

    mov [PAGE_DIR_TABLE_POS + 0xc00] , eax ;   将第一个页表地址存入页目录项0xc00,一个页表占用4个字节
;   也就是页表的0xc0000000 ~ 0xffffffff 共计 1GB 属于内核
;   0x00000000 ~ 0xbfffffff 共计 3GB 属于用户进程
    sub eax , 0x1000
    mov [PAGE_DIR_TABLE_POS + 4092] , eax  ;使最后一个目录项指向页目录表自己的地址

;   下面创建页表项(PTE)
    mov ecx , 256       ; 1M低端内存/每页大小4k = 256
    mov esi , 0
    mov edx , PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx + esi * 4] , eax   ;此时ebx已经在上面通过 eax 赋值为 0x101000,也就是第一个页表的位置
    add eax , 4096
    inc esi
    loop .create_pte

;创建内核其他页表的PDE
    mov eax , PAGE_DIR_TABLE_POS
    add eax , 0x2000        ;此时eax为第二个页表 的物理地址和属性
    or eax , PG_US_U | PG_RW_W | PG_P
    mov ebx , PAGE_DIR_TABLE_POS
    mov ecx , 254
    mov esi , 769
.craete_kernal_pde:
    mov [ebx + esi * 4] , eax
    inc esi
    add eax , 0x1000
    loop .craete_kernal_pde
    ret