    %include "boot.inc"
    section loader vstart=LOADER_BASE_ADDR
    ;LOADER_STACK_TOP equ LOADER_BASE_ADDR   ; 用于在保护模式下的栈顶
    ;jmp loader_start

;构建gdt以及其内部的描述符
    GDT_BASE:
                dd  0x00000000   ;gdt的起始地址
                dd  0x00000000

    CODE_DESC:
                dd  0x0000FFFF        ;code段描述符的起始地址    代码段
                dd  DESC_CODE_HIGH4

    DATA_STACK_DESC:
                dd  0x0000FFFF  ; 数据段和栈段描述符的起始地址
                dd  DESC_DATA_HIGH4

    VIDEO_DESC:
                dd  0x80000007          ;0x8000000;limit=(0xbffff-0xb8000)/4096=0x7ff     ;显存段描述符
                dd  DESC_VIDEO_HIGH4    ; 此时dpl为0，表示用户级

    GDT_SIZE    equ  $ - GDT_BASE
    GDT_LIMIT   equ  GDT_SIZE - 1

    times 60 dq 0     ;此处预留60个描述符 ，防止后面扩展段时出错以便后面扩展

    SELECTOR_CODE   equ (0x0001 << 3) + TI_GDT + RPL0       ;相当于(CODE_DESC - GDT_BASE) / 8 + TI_GDT + RPL0
    SELECTOR_DATA   equ (0x0002 << 3) + TI_GDT + RPL0
    SELECTOR_VIDEO  equ (0x0003 << 3) + TI_GDT + RPL0

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
    ards_buf times 244 db 0         ; db 定义字节类型
    ards_nr dw 0                    ; dw 定义字类型
       ;loadermsg db '2 loader in real.'

       loader_start:

        ;int 15h eax = 0000E820h , edx = 534D4150h ('SMAP') 获取内存布局
        xor ebx, ebx ;ebx的值为0, xor指令用于清零
        mov edx, 0x534d4150
        mov di, ards_buf
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
        mov ebx ,ards_buf
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
        ;mov byte [gs:0], 'D'
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
        mov ah ,0x88
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

    jmp dword SELECTOR_CODE:p_mode_start   ; 跳转到保护模式代码段 刷新流水线

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


;------ 加载kernel ------
    mov eax , KERNEL_START_SECTOR   ;kernel.bin 所在的扇区号
    mov ebx , KERNEL_BIN_BASE_ADDR  ;从磁盘读出后、写入到ebx指定内存
    mov ecx , 200                   ;读入的扇区数

    call rd_disk_m_32

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

    ;mov eax , SELECTOR_VIDEO
    ;mov gs , eax

    jmp SELECTOR_CODE:enter_kernel

;------ 跳转到kernel ------
    enter_kernel:
        call kernel_init
        mov esp , 0xc009f000
        jmp KERNEL_ENTRY_POINT
;mov byte [gs:160], 'G'

;jmp $

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
    mov eax, PAGE_DIR_TABLE_POS
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
    mov ecx, 256       ; 1M低端内存/每页大小4k = 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx+esi*4],edx   ;   将页表项的地址和属性写入页表
    add edx , 4096                  ;******** edx不是eax ********
    inc esi
    loop .create_pte

;创建内核其他页表的PDE
    mov eax , PAGE_DIR_TABLE_POS
    add eax , 0x2000        ;此时eax为第二个页表 的物理地址和属性
    or eax , PG_US_U | PG_RW_W | PG_P
    mov ebx , PAGE_DIR_TABLE_POS
    mov ecx , 254
    mov esi , 769           ;  769表示页目录表中的第769个目录项  指向 0xc0000000 ~ 0xffffffff 共计 1GB 属于内核
    ;   这段内核代码所有进程共享

.craete_kernal_pde:
    mov [ebx + esi * 4] , eax
    inc esi
    add eax , 0x1000
    loop .craete_kernal_pde
    ret

;-------- 将kernel.bin 中的segment 拷贝到编译的地址 ---------------------
kernel_init:
    xor eax , eax
    xor ebx , ebx   ;ebx记录程序头表地址
    xor ecx , ecx   ;cx记录程序头表中的 program header 的数量
    xor edx , edx   ;dx记录 program header 尺寸 ， 即e_phentsize

    mov dx , [KERNEL_BIN_BASE_ADDR + 42] ;偏移文件42字节处的属性是e_phentsize，表示program header的大小
    mov ebx , [KERNEL_BIN_BASE_ADDR + 28] ;偏移文件28字节处的属性是e_phoff，表示program header的偏移

    add ebx , KERNEL_BIN_BASE_ADDR
    mov cx , [KERNEL_BIN_BASE_ADDR + 44] ;偏移文件44字节处的属性是e_phnum，表示program header的数量

.each_segment:
    cmp byte [ebx + 0] , PT_NULL        ;若p_type为PT_NULL,说明此 program header 未使用
    je .PTNULL

    ;为函数 memcpy 压入参数 ， memcpy (void *es:p_dst , void *ds:p_src , int len)参数从右往左依次压入
    push dword [ebx + 16]           ;program header 中偏移16字节的地方是 p_filesz
    ;压入的是第三个参数len

    mov eax , [ebx + 4]          ;program header 中偏移4字节的地方是 p_offset
    add eax , KERNEL_BIN_BASE_ADDR ;加上kernel.bin被加载到的物理地址，eax为该段的物理地址
    push eax                     ;压入第二个参数p_src,源地址
    push dword [ebx + 8]         ;压入第一个参数p_dst,目标地址
    ;偏移程序头8字节的位置是 p_vaddr ，这就是目的地址

    call mem_cpy ;调用 memcpy 函数，将段从源地址复制到目标地址
    add esp , 12 ;清理栈中压入的参数
.PTNULL:
    add ebx , edx ; edx为program header的大小，即e_phentsize  ; ebx指向下一个program header
    loop .each_segment ;循环处理下一个段
    ret

;------- 逐字节拷贝 mem_cpy (void *es:p_dst , void *ds:p_src , int len) -------
;输入:栈中的三个参数
;输出:无
mem_cpy:
    cld
    push ebp
    mov ebp, esp
    push ecx        ;rep 指令用到了ecx寄存器，先将其压入栈中
    mov edi , [ebp + 8] ; dst
    mov esi , [ebp + 12] ; src
    mov ecx , [ebp + 16] ; len
    rep movsb   ;逐字节拷贝

    ;恢复环境
    pop ecx
    pop ebp
    ret

rd_disk_m_32:
;---------------------------------------------
;   从硬盘读取数据
;   eax=LBA扇区号
;   ebx=写入的内存地址
;   ecx=读取的扇区数
;-------------------------------------------------------------------------------
			   ;功能:读取硬盘n个扇区
;rd_disk_m_32:
;-------------------------------------------------------------------------------
							 ; eax=LBA扇区号
							 ; ebx=将数据写入的内存地址
							 ; ecx=读入的扇区数
      mov esi,eax	   ; 备份eax
      mov di,cx		   ; 备份扇区数到di
;读写硬盘:
;第1步：设置要读取的扇区数
      mov dx,0x1f2
      mov al,cl
      out dx,al            ;读取的扇区数

      mov eax,esi	   ;恢复ax

;第2步：将LBA地址存入0x1f3 ~ 0x1f6

      ;LBA地址7~0位写入端口0x1f3
      mov dx,0x1f3
      out dx,al

      ;LBA地址15~8位写入端口0x1f4
      mov cl,8
      shr eax,cl
      mov dx,0x1f4
      out dx,al

      ;LBA地址23~16位写入端口0x1f5
      shr eax,cl
      mov dx,0x1f5
      out dx,al

      shr eax,cl
      and al,0x0f	   ;lba第24~27位
      or al,0xe0	   ; 设置7～4位为1110,表示lba模式
      mov dx,0x1f6
      out dx,al

;第3步：向0x1f7端口写入读命令，0x20
      mov dx,0x1f7
      mov al,0x20
      out dx,al

;;;;;;; 至此,硬盘控制器便从指定的lba地址(eax)处,读出连续的cx个扇区,下面检查硬盘状态,不忙就能把这cx个扇区的数据读出来

;第4步：检测硬盘状态
  .not_ready:		   ;测试0x1f7端口(status寄存器)的的BSY位
      ;同一端口,写时表示写入命令字,读时表示读入硬盘状态
      nop
      in al,dx
      and al,0x88	   ;第4位为1表示硬盘控制器已准备好数据传输,第7位为1表示硬盘忙
      cmp al,0x08
      jnz .not_ready	   ;若未准备好,继续等。

;第5步：从0x1f0端口读数据
      mov ax, di	   ;以下从硬盘端口读数据用insw指令更快捷,不过尽可能多的演示命令使用,
			   ;在此先用这种方法,在后面内容会用到insw和outsw等

      mov dx, 256	   ;di为要读取的扇区数,一个扇区有512字节,每次读入一个字,共需di*512/2次,所以di*256
      mul dx
      mov cx, ax
      mov dx, 0x1f0
  .go_on_read:
      in ax,dx
      mov [ebx], ax
      add ebx, 2
			  ; 由于在实模式下偏移地址为16位,所以用bx只会访问到0~FFFFh的偏移。
			  ; loader的栈指针为0x900,bx为指向的数据输出缓冲区,且为16位，
			  ; 超过0xffff后,bx部分会从0开始,所以当要读取的扇区数过大,待写入的地址超过bx的范围时，
			  ; 从硬盘上读出的数据会把0x0000~0xffff的覆盖，
			  ; 造成栈被破坏,所以ret返回时,返回地址被破坏了,已经不是之前正确的地址,
			  ; 故程序出会错,不知道会跑到哪里去。
			  ; 所以改为ebx代替bx指向缓冲区,这样生成的机器码前面会有0x66和0x67来反转。
			  ; 0X66用于反转默认的操作数大小! 0X67用于反转默认的寻址方式.
			  ; cpu处于16位模式时,会理所当然的认为操作数和寻址都是16位,处于32位模式时,
			  ; 也会认为要执行的指令是32位.
			  ; 当我们在其中任意模式下用了另外模式的寻址方式或操作数大小(姑且认为16位模式用16位字节操作数，
			  ; 32位模式下用32字节的操作数)时,编译器会在指令前帮我们加上0x66或0x67，
			  ; 临时改变当前cpu模式到另外的模式下.
			  ; 假设当前运行在16位模式,遇到0X66时,操作数大小变为32位.
			  ; 假设当前运行在32位模式,遇到0X66时,操作数大小变为16位.
			  ; 假设当前运行在16位模式,遇到0X67时,寻址方式变为32位寻址
			  ; 假设当前运行在32位模式,遇到0X67时,寻址方式变为16位寻址.

      loop .go_on_read
      ret
