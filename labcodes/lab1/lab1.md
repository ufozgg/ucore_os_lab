# LAB1 实验报告

## 练习1：理解通过make生成执行文件的过程。

### 操作系统镜像文件ucore.img是如何一步一步生成的？(需要比较详细地解释Makefile中每一条相关命令和命令参数的含义，以及说明命令导致的结果)

根据题目介绍的方法，利用`make "V=""`观察makefile运行过程。

内容如下：

* 先编译kern目录下的init、libs、debug、driver、trap文件夹内的多个文件，生成相应的二进制文件（.o文件），使用的参数列举如下：

** `-fno-builtin`不使用C语言的内建函数；
** `-Wall`输出所有警告信息，有利于观察各种Warning；
** `-ggdb`提供gdb的支持；
** `-m32`生成32位二进制文件；
** `-gstabs`生成gstab格式的调试信息；
** `-nostdinc`不在标准库中查找头文件（之后要独立于编译用的系统来运行）；
** `-fno-stack-protector`禁用堆栈保护，堆栈保护会调用某些标准库函数，与生成操作系统的目的相悖。

* 运行ld连接器内核，生成可执行文件`bin/kernel`，使用的参数列举如下：

** `-m elf_i386`生成i386代码；
** `-nostdlib`同上；
** `-T tools/kernel.ld`使用其中自定义方式分配存储空间，将入口设为`kern_init`；

* 编译`boot`文件夹下的文件生成引导，大部分参数同上文，新增参数`-Os`gcc优化开关。

* 对boot文件进行链接，参数如下：

** `-N`设置读写权限为可读写；
** `-e start`入口函数设置；
** `-Ttext 0x7C00`设置代码起始地址，从这个地方开始引导、

* 使用OBJDUMP、OBJCOPY，得到Bootloader的代码段。
* 利用得到的代码段，生成512字节的扇区镜像。
* 使用dd工具，创建`ucore.img`镜像，参数为：
** `if`输入文件；
** `of`输出文件（ucore.img）；
** `count`block数量；
** `conv=notrunc`不缩小目标文件；
** `seek`拷贝起始地址。

常用的make方法还包括：
* clean 清空历史数据；
* debug 进入debug模式；
* qemu 生成仿真系统并在其中调试；
* grade 运行评分脚本；
* handin 生成提交文件（本次直接提交git仓库，无需生成）

### 一个被系统认为是符合规范的硬盘主引导扇区的特征是什么？

主引导区大小是512字节，第511和512位为`0x55AA`，为结束标志。通常还位于硬盘的起始位置。

## 练习2：使用qemu执行并调试lab1中的软件。（要求在报告中简要写出练习过程）

### 从CPU加电后执行的第一条指令开始，单步跟踪BIOS的执行。
参照附录B， 修改 lab1/tools/gdbinit，加入以下语句：
`
set architecture i8086
target remote :1234
`
在lab1下执行`make debug`

可以看到，第一条运行的指令为：`0xffff0`。

查看后续指令，可以看到跳转语句：
```
   0xffff0:     ljmp   $0xf000,$0xe05b
```

在跳转后，可见：
```
0xfe05b:     cmpl   $0x0,%cs:0x6c48
   0xfe062:     jne    0xfd2e1
   0xfe066:     xor    %dx,%dx
   0xfe068:     mov    %dx,%ss
   0xfe06a:     mov    $0x7000,%esp
   0xfe070:     mov    $0xf3691,%edx
   0xfe076:     jmp    0xfd165
   0xfe079:     push   %ebp
   0xfe07b:     push   %edi
   0xfe07d:     push   %esi
```

### 在初始化位置0x7c00设置实地址断点,测试断点正常。

在gdb中输入命令：
```
b 0x7c00
c
```
可以发现，dgb在断点处（程序入口处）跳出。

### 从0x7c00开始跟踪代码运行,将单步跟踪反汇编得到的代码与bootasm.S和 bootblock.asm进行比较。

执行的代码和两个文件中的代码逻辑上一致，说明编译正常，符合预期。

### 自己找一个bootloader或内核中的代码位置，设置断点并进行测试。

选择在trap.c中的print_tick处设置断点。

结果正常，可以逐句运行。

也可以查看变量，查看变量时，每次tick计数器增加1，符合预期。

## 练习3：分析bootloader进入保护模式的过程。（要求在报告中写出分析）

阅读小节“保护模式和分段机制”和lab1/boot/bootasm.S源码，了解如何从实模式切换到保护模式后，分析该过程。

该工作在 bootasm.S 中完成，具体代码见该文件。

在Boot时，CPU使用的模式为实模式，在设计用到该内容的第一代计算机时，还是1985年，当时的存储容器都很小，1M的寻址空间超出容器大小，因此，只有20位地址线。并为了兼容，设计了A20Gate==0的规则。

之后，寻址空间虽然扩大，但A20恒为0的规定却导致了CPU只能访问奇数（Gate20==0）块的内存，因此，先要取消掉A20==0的设计，才能进入保护模式。

然后，操作系统读取GDT表，初始化。

最后，使能CR0寄存器的PE位，进入到32位地址（保护模式）。

## 练习4：分析bootloader加载ELF格式的OS的过程。（要求在报告中写出分析）

### bootloader如何读取硬盘扇区的？

读取磁盘扇区的方法较为强力，即等待磁盘空闲时读取。

在bootloader中，IO地址在`0x1f[0-7]`，循环访问，等待其空闲。

按照磁盘的读取规则，向以下端口写入参数：

* `0x1f2`读取扇区数量；
* `0x1f[3-6]`扇区号；
* `0x1f7`写入`0x20`表示读取。

然后等待读取完成（磁盘变为空闲）即可。

### bootloader是如何加载ELF格式的OS？

读取ELF文件的前8个扇区，并检测其中的magic值，以确定是否是一个合理的OS。

从ELF头读取Program Header的偏移和长度。

拷贝上一点中描述的所有数据至内存。

跳转到起始位置执行操作系统。

## 练习5：实现函数调用堆栈跟踪函数 （需要编程）

编写程序如下：

kdebug.c
```
uint32_t ebp,eip;
	uint32_t* last_ebp;
	uint32_t i,j;
	ebp = read_ebp();//读取当前活跃栈信息
	eip = read_eip();
	for(i=0;i<STACKFRAME_DEPTH;++i)//遍历栈中每一层
	{
		cprintf("ebp:0x%08x eip:0x%08x args:",ebp,eip);//输出栈中各项参数
		uint32_t *arg = (uint32_t*)ebp + 2;
		for(j=0;j<4;++j)
			cprintf("0x%08x ",arg[j]);
		cprintf("\n");
		print_debuginfo(eip-1);
		last_ebp = (uint32_t*)ebp;
		//cprintf("EQ 0x%08x 0x%08x\n",arg[0],last_ebp[2]);
		//assert(arg[0]==last_ebp[2]);
		ebp = last_ebp[0];//读取上一层栈中信息
		eip = last_ebp[1];
	}
```

输出内容如下，与参考答案一致：

```
ebp:0x00007b38 eip:0x00100a37 args:0x00010094 0x00010094 0x00007b68 0x00100084 
    kern/debug/kdebug.c:298: print_stackframe+21
ebp:0x00007b48 eip:0x00100d35 args:0x00000000 0x00000000 0x00000000 0x00007bb8 
    kern/debug/kmonitor.c:125: mon_backtrace+10
ebp:0x00007b68 eip:0x00100084 args:0x00000000 0x00007b90 0xffff0000 0x00007b94 
    kern/init/init.c:48: grade_backtrace2+19
ebp:0x00007b88 eip:0x001000a6 args:0x00000000 0xffff0000 0x00007bb4 0x00000029 
    kern/init/init.c:53: grade_backtrace1+27
ebp:0x00007ba8 eip:0x001000c3 args:0x00000000 0x00100000 0xffff0000 0x00100043 
    kern/init/init.c:58: grade_backtrace0+19
ebp:0x00007bc8 eip:0x001000e4 args:0x00000000 0x00000000 0x00000000 0x00103560 
    kern/init/init.c:63: grade_backtrace+26
ebp:0x00007be8 eip:0x00100050 args:0x00000000 0x00000000 0x00000000 0x00007c4f 
    kern/init/init.c:28: kern_init+79
ebp:0x00007bf8 eip:0x00007d6e args:0xc031fcfa 0xc08ed88e 0x64e4d08e 0xfa7502a8 
    <unknow>: -- 0x00007d6d --
ebp:0x00000000 eip:0x00007c4f args:0xf000e2c3 0xf000ff53 0xf000ff53 0xf000ff53 
    <unknow>: -- 0x00007c4e --
ebp:0xf000ff53 eip:0xf000ff53 args:0x00000000 0x00000000 0x00000000 0x00000000 
    <unknow>: -- 0xf000ff52 --
ebp:0x00000000 eip:0x00000000 args:0xf000e2c3 0xf000ff53 0xf000ff53 0xf000ff53 
    <unknow>: -- 0xffffffff --
ebp:0xf000ff53 eip:0xf000ff53 args:0x00000000 0x00000000 0x00000000 0x00000000 
    <unknow>: -- 0xf000ff52 --
ebp:0x00000000 eip:0x00000000 args:0xf000e2c3 0xf000ff53 0xf000ff53 0xf000ff53 
    <unknow>: -- 0xffffffff --
ebp:0xf000ff53 eip:0xf000ff53 args:0x00000000 0x00000000 0x00000000 0x00000000 
    <unknow>: -- 0xf000ff52 --
ebp:0x00000000 eip:0x00000000 args:0xf000e2c3 0xf000ff53 0xf000ff53 0xf000ff53 
    <unknow>: -- 0xffffffff --
ebp:0xf000ff53 eip:0xf000ff53 args:0x00000000 0x00000000 0x00000000 0x00000000 
    <unknow>: -- 0xf000ff52 --
ebp:0x00000000 eip:0x00000000 args:0xf000e2c3 0xf000ff53 0xf000ff53 0xf000ff53 
    <unknow>: -- 0xffffffff --
ebp:0xf000ff53 eip:0xf000ff53 args:0x00000000 0x00000000 0x00000000 0x00000000 
    <unknow>: -- 0xf000ff52 --
ebp:0x00000000 eip:0x00000000 args:0xf000e2c3 0xf000ff53 0xf000ff53 0xf000ff53 
    <unknow>: -- 0xffffffff --
ebp:0xf000ff53 eip:0xf000ff53 args:0x00000000 0x00000000 0x00000000 0x00000000 
    <unknow>: -- 0xf000ff52 --

```
各项内容如下：

* ebp 表示栈的基地址
* eip 调用结束后下一条pc指令的位置
* args 调用参数
* <unknow>等内容，是代码中语句的位置。

## 练习6：完善中断初始化和处理 （需要编程）

### 中断描述符表（也可简称为保护模式下的中断向量表）中一个表项占多少字节？其中哪几位代表中断处理代码的入口？

占了8字节。中断处理代码入口为前32位。

### 请编程完善kern/trap/trap.c中对中断向量表进行初始化的函数idt_init。在idt_init函数中，依次对所有中断入口进行初始化。使用mmu.h中的SETGATE宏，填充idt数组内容。每个中断的入口由tools/vectors.c生成，使用trap.c中声明的vectors数组即可。

实现代码和说明如下：

```
extern uintptr_t __vectors[256];//vectors地址
	uintptr_t i;
	for(i=0;i<256;++i)//遍历所有表项
		SETGATE(idt[i],0,KERNEL_CS,__vectors[i], DPL_KERNEL);//依次填写表项为vectors中对应描述的位置。
	SETGATE(idt[T_SWITCH_TOK],0,KERNEL_CS, __vectors[T_SWITCH_TOK],DPL_USER);//READ ANS
    lidt(&idt_pd);
     /* LAB1 2015011371 : STEP 2 */
```

### 请编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写trap函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print_ticks子程序，向屏幕上打印一行文字”100 ticks”。

整个实验最简单的部分，参照注释对ticks进行维护即可。

```
    case IRQ_OFFSET + IRQ_TIMER:
		++ticks;
		if(ticks%100 == 0)
			print_ticks(ticks);
        /* LAB1 2015011371 : STEP 3 */
```

### 挑战

需要修改3处内容：

trap.c
```
//LAB1 CHALLENGE 1 : 2015011371 you should modify below codes.
    case T_SWITCH_TOU:
		if(tf->tf_cs != USER_CS)//如果不在用户态
		{
			usr = *tf;
			usr.tf_cs = USER_CS;//生成合理的trapframe
			usr.tf_ds = USER_DS;//将用户态参数放入trapframe
			usr.tf_es = USER_DS;//将状态设置为trapeframe
			usr.tf_ss = USER_DS;
			usr.tf_esp = tf + 76;
			usr.tf_eflags |= FL_IOPL_MASK;
			*((uint32_t *)tf - 1) = &usr;//READ ANS
		}
		break;
    case T_SWITCH_TOK:
		if(tf->tf_cs != KERNEL_CS)//如果不在内核态
		{
			tf->tf_cs = KERNEL_CS;//与切换到用户态类似
			tf->tf_ds = KERNEL_DS;
			tf->tf_es = KERNEL_DS;
			tf->tf_ss = KERNEL_DS;
			if(tf->tf_eflags & FL_IOPL_MASK)
				tf->tf_eflags ^= FL_IOPL_MASK;
			memmove(tf->tf_esp-76,tf,76);
			*((uint32_t *)tf - 1) = tf->tf_esp - 76;
		}
        break;
```

init.c
```
//LAB1: CAHLLENGE 1 If you try to do it, uncomment lab1_switch_test()
    // user/kernel mode switch test
    lab1_switch_test();//去掉这句话的注释
```

init.c
```
static void
lab1_switch_to_user(void) {
    //LAB1 CHALLENGE 1 : DONE
	//内联汇编，进行状态切换
	asm volatile (
	    "sub $0x8, %%esp \n"
	    "int %0 \n"
	    "movl %%ebp, %%esp"
	    : 
	    : "i"(T_SWITCH_TOU)
	);
}

static void
lab1_switch_to_kernel(void) {
    //LAB1 CHALLENGE 1 : DONE
	//内联汇编，操作底层进行状态切换
	asm volatile (
	    "int %0 \n"
	    "movl %%ebp, %%esp \n"
	    : 
	    : "i"(T_SWITCH_TOK)
	);
}
```

## 对比参考答案

实现大部分地方与参考答案逻辑一致，许多写法均为等价方法，没有观察到本质区别。

但以下例外：

* idt_init的实现中，门的参数被设为KERNEL_CS，而参考答案中为GD_KTEXT，二者均能正确执行，但我认为我的写法更为标准。

## 知识点总结

* 练习1：Makefile的基础知识复习；
* 练习2：gdb调试工具的基础，且初步熟悉代码；
* 练习3：保护模式与实模式，Bootloader的运行机制，GDT表的应用与设计思路；
* 练习4：bootloader读取磁盘扇区，加载OS的方法；
* 练习5：实现代码，用于观察与分析堆栈信息；
* 练习6：时钟中断的处理方法；
* 挑战实验：用户态与内核态的切换方法。

## 效果符合预期，如下

```
Check Output:            (2.3s)
  -check ring 0:                             OK
  -check switch to ring 3:                   OK
  -check switch to ring 0:                   OK
  -check ticks:                              OK
Total Score: 40/40

```
## 效果符合预期，如下

```

```
