# 变量定义
BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = kernel
DEVICE_DIR = device
LIB_DIR = lib
THREAD_DIR = thread
USERPROG_DIR = userprog
FS_DIR = fs
SHELL_DIR = shell
COMMAND_DIR = command
OUTPUT_IMG = /home/future/os/code/kernel.img
BOCHS_CONFIG = /home/future/os/code/bochsrc.disk
ENTRY_POINT = 0xc0001500

AS = nasm
CC = gcc
LD = ld
ASFLAGS = -I $(BOOT_DIR)/include/ -f elf
CFLAGS = -m32 -I $(LIB_DIR)/kernel/ -I $(LIB_DIR)/ -I $(KERNEL_DIR)/ \
         -I $(LIB_DIR)/user/ -I $(THREAD_DIR)/ -I $(USERPROG_DIR)/ \
		 -I $(DEVICE_DIR)/ -I $(FS_DIR)/ -I $(SHELL_DIR)/\
         -c -fno-builtin -fno-stack-protector
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main

# 引导程序对象文件
BOOT_OBJS = $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin

# 内核对象文件（包括新增的 console, ioqueue, keyboard 和 sync）
KERNEL_OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/init.o \
              $(BUILD_DIR)/timer.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/memory.o \
              $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/list.o $(BUILD_DIR)/string.o \
              $(BUILD_DIR)/thread.o $(BUILD_DIR)/print.o $(BUILD_DIR)/switch.o \
              $(BUILD_DIR)/kernel.o $(BUILD_DIR)/console.o $(BUILD_DIR)/ioqueue.o \
              $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/process.o \
			  $(BUILD_DIR)/tss.o  $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall_init.o \
			  $(BUILD_DIR)/stdio.o $(BUILD_DIR)/ide.o $(BUILD_DIR)/stdio_kernel.o \
			  $(BUILD_DIR)/fs.o $(BUILD_DIR)/file.o $(BUILD_DIR)/dir.o $(BUILD_DIR)/inode.o \
			  $(BUILD_DIR)/fork.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/assert.o \
			  $(BUILD_DIR)/buildin_cmd.o $(BUILD_DIR)/exec.o  $(BUILD_DIR)/wait_exit.o \
			  $(BUILD_DIR)/pipe.o
#   $(BUILD_DIR)/prog_no_arg.o

# 目标规则
.PHONY: all clean run

all: $(OUTPUT_IMG)

# 编译引导程序
$(BUILD_DIR)/mbr.bin: $(BOOT_DIR)/mbr.S
	$(AS) -I $(BOOT_DIR)/include/ -o $@ $<

$(BUILD_DIR)/loader.bin: $(BOOT_DIR)/loader.S
	$(AS) -I $(BOOT_DIR)/include/ -o $@ $<

# 编译 C 源文件
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(DEVICE_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(LIB_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(LIB_DIR)/kernel/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(LIB_DIR)/user/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(THREAD_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(USERPROG_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(FS_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(SHELL_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

# $(BUILD_DIR)/%.o: $(COMMAND_DIR)/%.c
# $(AS) $(ASFLAGS) -o $@ $<

# 编译汇编源文件
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.S
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(LIB_DIR)/kernel/%.S
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(THREAD_DIR)/%.S
	$(AS) $(ASFLAGS) -o $@ $<



# 链接内核
$(BUILD_DIR)/kernel.bin: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# 写入硬盘镜像
$(OUTPUT_IMG): $(BOOT_OBJS) $(BUILD_DIR)/kernel.bin
	dd if=$(BUILD_DIR)/mbr.bin of=$(OUTPUT_IMG) bs=512 count=1 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=$(OUTPUT_IMG) bs=512 count=4 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$(OUTPUT_IMG) bs=512 count=200 seek=9 conv=notrunc

# 清理生成的文件
clean:
	rm -f $(BUILD_DIR)/*

# 运行 Bochs 模拟器
run: all
	bochs -f $(BOCHS_CONFIG) -q
