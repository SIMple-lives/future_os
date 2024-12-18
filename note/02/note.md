# 02

## 载入内存

- cpu的硬件电路只能运行处于内存中的程序
  - 优点:速度快,容量大,统一不同的存储介质
- 程序载入内存的步骤
  1. 程序被加载器加载到内存某个区域
  2. CPU的`cs:ip`寄存器指向该程序的起始地址

------



## BIOS

`Base Input & Outpur System`

地址总线决定了我们访问那里、访问什么、以及访问范围的关键.

只读存储器`ROM`、将`BIOS`代码写入`ROM` 、被映射在低端1MB的顶部.

- Intel 8086 有 20 条地址线，可以访问 1MB 的内存空间，即 2 的 20 次方=1048576=1MB，地址范围是 0x00000 到 0xFFFFF
  - 但这20条地址总线**不是全部都给内存条使用**
  - 20条中一部分给外设，一部分给显存，一部分给...剩下的可用地址给内存条，也就是物理内存
  - 所以：32位机，就算安装了4GB内存条，但显示其内存也只有3.8GB左右

![image-20240730204222074](/home/future/图片/image-20240730204222074.png)

- 顶部的 0xF0000～0xFFFFF，这 64KB 的内存是 ROM，存的是 **BIOS 的代码**
- BIOS 的主要工作是**检测、初始化硬件**（硬件自己提供了一些初始化的功能调用，BIOS 直接调用就好了）
- BIOS 在内存中的 0x000 至 0x3FF 区域**建立中断向量表**，可以通过**int 中断号**来实现相关的**硬件调用**，这是**`对硬件的 IO 操作，也就是输入输出`** ----> （解释了为什么 BIOS 叫做基本输入输出系统）
- 0～0x9FFFF处是`DRAM`、即动态随即访问内存，我们所装的内存就是`DRAM`。

### BIOS的加载过程

1. BIOS是**计算机启动时第一个运行的软件**，它存储在只读存储器（ROM）中
2. **硬件加载**：BIOS 由硬件加载（ ROM 通过地址映射在低端 1MB 内存的顶部（地址 0xF0000 至 0xFFFFF））
3. **入口地址**：BIOS 的入口地址是 0xFFFF0。开机时，CPU 的**段寄存器（cs）和指令指针（ip）被强制初始化为 0xF000 和 0xFFF0**
4. **实模式下的地址计算**：在实模式下，段地址需要乘以 16，0xF000:0xFFF0 的**物理地址为 0xFFFF0**

### BIOS初始化过程

1. **跳转指令**：在 0xFFFF0 处的**跳转指令**（如 `jmp far f000:e05b`）指向 BIOS 代码的**实际位置**
2. **硬件检测**：BIOS 初始化后，会检测内存、显卡等硬件，当检测通过并初始化好硬件后，在内存中的 0x000 至 0x3FF 区域**建立中断向量表（IVT）**

### BIOS最后一项任务

1. 校验启动盘的**MBR**（主引导记录）
2. 检查MBR末尾的两个字节（魔数0x55和0xaa）来确认该扇区中存在可执行程序

------

## MBR

`$`和`$$`是编译器`NASM`预留的关键字，用来表示当前行和本`section`的地址，起到了标号的作用。

`$`是本行的地址

`$$`是本`section`的起始地址

## `NASM`简单用法

```smarty
nasm -f <format><filename> [-o <output>]
```

-o 就是指定输出可执行文件的名称

-f 用来指定输出文件的格式

## MBR程序

```asm
mov ax,0x1301
;13对应的是ah寄存器，调用0x13号子功能，01对应的是al寄存器、表示的写字符方式
```

