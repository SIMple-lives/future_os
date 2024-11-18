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

    ;以下是gdt的指针，前两字节是gdt界限，后4字节是gdt起始地址

    gdt_ptr dw GDT_LIMIT
            dd GDT_BASE
       loadermsg db '2 loader in real.'

       loader_start:

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
    mov sp ,LOADER_BASE_ADDR
    mov bp ,loadermsg ;
    mov cx ,17 ;cx=字符串长度
    mov ax ,0x1301 ;AH=13H,AL=01h
    mov bx ,0x001f ;页号为0(BH = 0) 蓝底粉红字(BL = 1fh)
    mov dx ,0x1800     ;这里决定输出到第21行，列号为0。也就是最底下一行的地方
    int 0x10 ;调用BIOS

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


[bits 32]
p_mode_start:
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP
    mov ax,SELECTOR_VIDEO
    mov gs,ax

    mov byte [gs:160], 'P'

    jmp $