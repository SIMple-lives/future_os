[bits 32]
%define ERROR_CODE nop      ;若在相关的异常中已经压入栈  错误码，为保持栈中格式统一，这里不做操作
%define ZERO push 0              ;若在相关的异常中没有压入错误码，为统一栈中格式，手工压入一个0

extern put_str
extern idt_table              ;声明外部函数

section .data
;intr_str db "interrupt occur!", 0xa, 0  ; 中断信息字符串 0xa 是换行符 0 是字符串结束符
global intr_entry_table
intr_entry_table:

%macro VECTOR 2
section .text
intr%1entry:                ;每个中断处理程序都要压入中断向量号，所以一个中断类型一个中断处理程序

    %2
    push ds
    push es
    push fs
    push gs
    pushad
    ;add esp, 4              ;跳过参数

    ;如果是从片上进入的中断，除了往从片上发送EOI外，还要往主片上发送EOI
    mov al, 0x20            ;向主片发送EOI
    out 0xa0, al            ;向从片发送EOI
    out 0x20, al

    ;add esp, 4              ;跳过错误码
    ;iret                    ;中断返回,32位下等同指令iretd
    push %1                  ;不管idt_table中是否压入错误码，这里都压入中断向量号
    call [idt_table + %1*4]  ;调用中断处理程序
    jmp intr_exit

section .data
    dd intr%1entry            ;每个中断处理程序都要压入中断向量号，所以一个中断类型一个中断处理程序,形成intr_entry_table数组
%endmacro

section .text
global intr_exit
intr_exit:
;以下是恢复上下文环境
    add esp, 4              ;跳过中断向量号
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4              ;跳过错误码
    iretd

VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO
VECTOR 0x20,ZERO    ;时钟中断对应的入口

;8259A的中断屏蔽寄存器
VECTOR 0x21,ZERO	;键盘中断对应的入口
VECTOR 0x22,ZERO	;级联用的
VECTOR 0x23,ZERO	;串口2对应的入口
VECTOR 0x24,ZERO	;串口1对应的入口
VECTOR 0x25,ZERO	;并口2对应的入口
VECTOR 0x26,ZERO	;软盘对应的入口
VECTOR 0x27,ZERO	;并口1对应的入口
VECTOR 0x28,ZERO	;实时时钟对应的入口
VECTOR 0x29,ZERO	;重定向
VECTOR 0x2a,ZERO	;保留
VECTOR 0x2b,ZERO	;保留
VECTOR 0x2c,ZERO	;ps/2鼠标
VECTOR 0x2d,ZERO	;fpu浮点单元异常
VECTOR 0x2e,ZERO	;硬盘
VECTOR 0x2f,ZERO	;保留

;;;;;;    0x80号中断     ;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
    ;保存上下文环境
    push 0              ;压入0,使栈中格式统一，压入错误码

    push ds
    push es
    push fs
    push gs
    pushad              ;pushad指令将EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI依次压入栈中

    push 0x80            ;压入中断号

    ;为系统调用压入参数
    push edx            ;第三个参数
    push ecx            ;第二个参数
    push ebx            ;第一个参数          ;寄存器名称写错

    ;调用子功能处理函数
    call [syscall_table + eax*4]    ;编译器会在栈中根据c函数声明匹配正确数量的参数
    add esp, 12                     ;跳过上面三个参数

    ;将call调用后的返回值存入待当前内核栈中eax的位置
    mov [esp + 8*4], eax
    jmp intr_exit                   ;返回恢复上下文