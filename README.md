# future_os



## 介绍

`future_os`是一个基于 x86 架构的教学型操作系统内核，参考《操作系统真相还原》一书实现。项目从零开始构建了一个支持多任务调度、内存管理、文件系统和简单命令行交互的操作系统，旨在深入理解操作系统底层原理。



## 特性

* **Bochs深度集成**

​	提供配置的`bochsrc`文件，支持断点调试与状态检查

- **多任务调度**  
  支持基于时间片轮转的进程调度，可运行多个用户程序。
- **物理内存管理**  
  实现基于位图的物理内存分配与回收。
- **文件系统**  
  支持 FAT12 文件系统，提供文件读写接口。
- **命令行交互**  
  内置简单 Shell，支持基础命令如 `ls`, `cat`, `clear` 等。
- **系统调用**  
  提供 `fork`, `exit`, `write` 等系统调用接口。



## 快速开始

### 环境依赖

- **编译工具链**  
  
  - NASM (>= 2.15)
  - GCC (支持 `-m32` 的 x86 交叉编译工具链)
  - GNU Make
- **bochs 模拟器**  

  推荐启用调试支持.

  ```bash
    # Ubuntu/Debian
    sudo apt install bochs bochs-x
    
    # macOS (Homebrew)
    brew install bochs
    
    # Arch Linux
    sudo pacman -S bochs
  ```

### 构建与运行

1. **克隆仓库**

   ```bash
   git clone https://github.com/SIMple-lives/future_os.git
   cd future_os
   ```

2. [环境搭建](https://github.com/SIMple-lives/future_os/blob/main/note/%E4%B8%80.%E7%8E%AF%E5%A2%83%E6%90%AD%E5%BB%BA.md)

3. 修改`bochsrc`的配置信息，与本地相匹配

4. 修改`code/Makefile`	

```bash
make run
```

