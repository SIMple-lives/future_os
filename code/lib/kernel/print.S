TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

section .data
put_int_buffer dq  0        ;定义8一个缓冲区，用于存储要打印的整数

[bits 32]
section .text
;---------- putchar -----------------
;功能描述：把栈中的一个字符写入光标所在的位置
;------------------------------------
global put_char
put_char:
    pushad      ;备份而32位寄存器环境;将所有的32位的寄存器进行备份
    ;需要保证gs中为正确的视频段选择子，为保险起见，每次打印时都需要为gs赋值
    mov ax, SELECTOR_VIDEO  ;不能把立即数送入段寄存器
    mov gs, ax

;------ 获取光标位置 ------
;   首先获取高8位
    mov dx, 0x03d4      ;索引寄存器 因为miscellaneous寄存器的值为0x67,I/OAS位为1
    ;所以CRT Controller的索引寄存器为0x3d4，data寄存器为0x3d5
    mov al, 0x0e        ;用于提供光标位置的高8位
    out dx, al
    mov dx, 0x03d5      ;通过读写数据端口0x3d5来获得或设置光标位置
    in al, dx           ;得到了光标位置的高8位
    mov ah, al

;   然后获取低8位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al;
    mov dx, 0x03d5
    in al, dx           ;得到了光标位置的低8位
    ;ax寄存器由ah(高)+al(低)组成
    mov bx, ax          ;bx中存放光标位置
    ;在栈中获取待打印的字符
    mov ecx, [esp + 36] ;pushad压入4*8=32字节，再加上4字节esp，所以待打印的字符在[esp+36]处
    cmp cl, 0xd         ;CR是0x0d,LF是0x0a
    jz .is_carriage_return;回车
    cmp cl, 0xa
    jz .is_line_feed    ;换行

    cmp cl, 0x8         ;BS(backspace)是0x08
    jz .is_backspace
    jmp .put_other


.is_backspace:
;------ backspace的一点说明 ------
;本质上只要将光标向前移动一个显存位置即可，后面在输入的字符会自然覆盖此处的字符
;但是，如果在键入backspace后，在不键入新的字符，这时光标已经移动，但字符还留在原地
;所以在此处添加了空格或空字符0
    dec bx
    shl bx, 1       ;光标左移一位等于乘2
                    ;表示光标对应显存中的偏移字节
    mov byte [gs:bx], 0x20  ;将带删除的字节补为0或空格皆可
    inc bx
    mov byte [gs:bx], 0x07
    shr bx, 1
    jmp .set_cursor

.put_other:
    shl bx, 1   ;光标位置用2字节表示，将光标值乘2,对应显存中的偏移字节
    mov [gs:bx], cl     ;ASCII码写入显存
    inc bx
    mov byte[gs:bx], 0x07   ;字符属性
    shr bx, 1   ;恢复光标值
    inc bx      ;光标右移一位
    cmp bx, 2000
    jl .set_cursor  ;若光标值小于2000，表示未写到显存的最后，则去设置新的光标值

.is_line_feed:
.is_carriage_return:
;如果是CR(\r),只要把光标移到行首
    xor dx, dx  ;dx是被除数的高16位，清0,异或操作
    mov ax, bx  ;ax是被除数的低16位
    mov si, 80  ;由于效仿Linux,Linux中\n表示换行，\r表示回车
    div si
    sub bx, dx

.is_carriage_return_end:
    add bx, 80
    cmp bx, 2000
.is_line_feed_end:
    jl .set_cursor


;屏幕范围是0～24，滚屏的原理是搬运，将24行用空格填充
.roll_screen:
    cld
    mov ecx, 960    ;一共需要搬运1920个字符，3840字节，依次搬4字节，共960次
    mov esi, 0xc00b80a0 ;第一行行首;c00b80a0 //一定是0xc00b80a0
    mov edi, 0xc00b8000 ;第零行行首          //一定是0xc00b8000
    rep movsd   ;rep movsd是重复ecx次，将esi指向的内存搬移到edi指向的内存

;将最后一行填充为空白
    mov ebx, 3840   ;最后一行首字符的第一个字节偏移=1920*2
    mov ecx, 80     ;一行有80个字符
.cls:
    mov word [gs:ebx], 0x0720;0x0720表示空格，属性为7
    add ebx, 2
    loop .cls
    mov bx, 1920        ;将光标值重置为1920,最后一行的行首

.set_cursor:
;将光标值设为bx值
;------ 先设置高8位 ------
    mov dx, 0x3d4  ;索引寄存器
    mov al, 0x0e    ;用于提供光标位置的高8位
    out dx, al
    mov dx, 0x3d5   ;通过读写数据端口0x3d5来获得或设置光标位置
    mov al, bh
    out dx, al

;------ 再设置低8位 ------
    mov dx, 0x3d4
    mov al, 0x0f    ;用于提供光标位置的低8位
    out dx, al;
    mov dx, 0x3d5
    mov al, bl
    out dx, al
    .put_char_done:
    popad
    ret

;-----------------------------------
;put_str 通过put_char来打印以字符0结尾的字符串
;-----------------------------------
;输入:栈中参数为打印的字符串
;输出:无

global put_str
put_str:
;由于本函数只用到了ecx和ebx寄存器，所以需要保存ebx和ecx的值
    push ebx
    push ecx
    xor ecx, ecx ;清零ecx
    mov ebx, [esp+12] ;函数返回地址在加上备份的两个寄存器的值
    .goon:
    mov cl, [ebx] ;将字符串中的字符存入cl
    cmp cl, 0   ;如果处理到了字符串结尾，跳到结束处返回
    jz .str_over
    push ecx        ;为put_char函数传递参数
    call put_char
    add esp, 4  ;回收参数所占的栈空间
    inc ebx  ;指向下一个字符
    jmp .goon
    .str_over:
    pop ecx
    pop ebx
    ret

;------ 将小段字节需的数字变成对应的ascii后，倒置 ------
;输入:栈中参数为待打印的数字
;输出:在屏幕上打印16进制数字，并不会打印前缀0x,也不会打印空格
;-----------------------------------------------
global put_int
put_int:
    pushad
    mov ebp, esp
    mov eax, [ebp+36]   ;函数返回地址在加上备份的8个寄存器的值
    mov edx, eax
    mov edi, 7          ;指定在put_int_buffer中初始的偏移量
    mov ecx, 8          ;32位数字中，16进制数字的为数是8个
    mov ebx, put_int_buffer

;将32位数字按照十六进制的形式从低位到高位逐个处理，一共处理8个十六进制数字
.16based_4bits: ;每4为二进制是16进制数字的一位
    and edx, 0x0000000F ;解析十六进制数字的每一位，and与操作后，edx只有低四位有效
    cmp edx, 9  ;数字0~9和a~f需要分别处理成对应的字符
    jg .is_A2F
    add edx, '0' ;数字0~9，直接加上字符'0'的ascii码,ascii是8位大小，add求和操作后，edx低8位有效
    jmp .store
.is_A2F:
    sub edx, 10     ;A~F的数字减去10所得到的差，在加上字符A的ascii吗，便是A~F对应的ASCII码
    add edx, 'A'

;将每一位数字转换成对应的字符后，按照类似大端的顺序存储到缓冲区put_int_buffer
;高位字符放在低地址，低位字符要放在高地址，这样和大端字节序类似，只不过这里是字符序
.store:
;此时dl中应该是数字对应的字符的ASCII码
    mov [ebx + edi], dl
    dec edi
    shr eax, 4
    mov edx, eax
    loop .16based_4bits

;现在put_int_buffer中已全部是字符，打印之前
;把高位连续的字符去掉，比如把字符00123变成123
.ready_to_print:
    inc edi     ;此时edi退减为-1(0xffffffff),加1使其为0
.skip_prefix_0:
    cmp edi, 8  ;若已经比较第9个字符了，表示待打印的字符串全部为0
    je .full0
;找出连续的0字符，edi作为非0的最高位字符的偏移
.go_on_skip:
    mov cl, [put_int_buffer+edi]
    inc edi
    cmp cl, '0'
    je .skip_prefix_0   ;继续判断下以为字符是否为字符0
    dec edi             ;edi在上面的inc操作中指向了下一个字符
    jmp .put_each_num

.full0:
    mov cl, '0' ;若全部为0，则只打印一个0
.put_each_num:
    push ecx    ;此时cl中为可打印的字符
    call put_char
    add esp ,4
    inc edi
    mov cl, [put_int_buffer+edi]
    cmp edi ,8
    jl .put_each_num
    popad
    ret

global set_cursor
set_cursor: ;设置光标位置
    pushad
    mov bx,[esp + 36]
    ;;;;;;; 1 先设置高8位 ;;;;;;;;
       mov dx, 0x03d4			  ;索引寄存器
       mov al, 0x0e				  ;用于提供光标位置的高8位
       out dx, al
       mov dx, 0x03d5			  ;通过读写数据端口0x3d5来获得或设置光标位置
       mov al, bh
       out dx, al

    ;;;;;;; 2 再设置低8位 ;;;;;;;;;
       mov dx, 0x03d4
       mov al, 0x0f
       out dx, al
       mov dx, 0x03d5
       mov al, bl
       out dx, al
       popad
       ret

global cls_screen
cls_screen:
    pushad
    ;;;;;;;;;;;;;;
        ;由于用户程序的cpl为3,显存段的dpl为0,故用于显存段的选择子gs在低于自己特权的环境中为0,
        ;导致用户程序再次进入中断后,gs为0,故直接在put_str中每次都为gs赋值.
    mov ax, SELECTOR_VIDEO              ;不能直接把立即数送入gs,由ax中转
    mov gs, ax

    mov ebx, 0
    mov ecx, 80*25
    .cls:
       mov word [gs:ebx], 0x0720		  ;0x0720是黑底白字的空格键
       add ebx, 2
       loop .cls
       mov ebx, 0

     .set_cursor:				  ;直接把set_cursor搬过来用,省事
    ;;;;;;; 1 先设置高8位 ;;;;;;;;
       mov dx, 0x03d4			  ;索引寄存器
       mov al, 0x0e				  ;用于提供光标位置的高8位
       out dx, al
       mov dx, 0x03d5			  ;通过读写数据端口0x3d5来获得或设置光标位置
       mov al, bh
       out dx, al

    ;;;;;;; 2 再设置低8位 ;;;;;;;;;
       mov dx, 0x03d4
       mov al, 0x0f
       out dx, al
       mov dx, 0x03d5
       mov al, bl
       out dx, al
       popad
       ret