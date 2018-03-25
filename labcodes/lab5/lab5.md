# LAB5 实验报告

## 练习0：填写已有实验

需要修改之前的代码，使其能顺利合并：

在`trap.c`的`idt_init`中，增加
`SETGATE(idt[T_SYSCALL], 1, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);`
对`syscal`部分进行设置。

在`trap.c`的`trap_dispatch`中，增加
`current->need_resched = 1;`
即在始终中断时，将当前进程设置为`需要调度`，以切换进程。

在`proc.c`的`do_fork`中，修改父进程为`current`，同时设置进程链接

`proc->parent = current;`

`set_links(proc);`

合并成功，无异常现象。

相比于lab4，本实验新增了文件夹：`user`、`kern/syscal`。

## 练习1：加载应用程序并执行

需要实现的部分为`lode_icode`函数中的第6部分：设置用户环境，即对当前进程的trapframe进行填写。

需要填写的内容包括：

* `tf_cs`：用户态代码段寄存器，设为`USER_CS`即可；
* `tf_ds=tf_es=tf_ss`：用户态数据段寄存器，设置为`USER_DS`即可；
* `tf_esp`：用户态栈指针，设为`USTACKTOP`；
* `tf_eip`：用户态内核指针，设为`elf->e_entry`；
* `tf_eflags`：用户态权限信息，设为`FL_IF`。

### 代码实现：

```
	struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
	tf->tf_cs = USER_CS;
	tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
	tf->tf_esp = USTACKTOP;
	tf->tf_eip = elf->e_entry;
	tf->tf_eflags = FL_IF;
```

### 问题1：请在实验报告中描述当创建一个用户态进程并加载了应用程序后，CPU是如何让这个应用程序最终在用户态执行起来的。即这个用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过。

CPU需要让这个进程执行时，首先调用`proc_run`函数，这个函数的介绍在lab4的实验报告中已经分析过。

其中，最后执行的语句`switch_to`是进行了一个汇编程序的调用，其中完成了寄存器的存取。

在调用这个汇编程序时，会令正在执行的进程设置好，把寄存器置为我们需要的值，此时，找到执行地址为`forkret`函数，该函数会设置栈信息。

在执行完`iret`后，就开始了新进程的执行。

而在ucore中，在内核态和用户态新建进程的方式有所不同，前者通过`do_fork`新建，后者通过`sys_fork`；但二者最终都会调用`SYS_exec`。在`SYS_exec`之后，才算是真正意义上的新进程**本身**的执行。

`SYS_exec`会先读取ELF文件，设置各项信息，才能完成工作，开始执行新进程。

## 练习2：父进程复制自己的内存空间给子进程

这个练习编码量不大，需要修改`pmm.c`中的`copy_range`。

具体只需要执行以下三个步骤：

* 获得父进程页的虚拟地址
* 获得子进程页的虚拟地址
* 复制该页内容
* 将该页的存在告知页表

### 代码实现

```
		void* src_kvaddr = page2kva(page);
		void* dst_kvaddr = page2kva(npage);
		memcpy(dst_kvaddr,src_kvaddr,PGSIZE);
		ret = page_insert(to,npage,start,perm);
```

### 问题1：如何设计实现”Copy on Write 机制“，给出概要设计

Copy-on-Write机制是指如果有多个使用者对一个资源A（比如内存块）进行读操作，则每个使用者只需获得一个指向同一个资源A的指针，就可以该资源了。若某使用者需要对这个资源A进行写操作，系统会对该资源进行拷贝操作，从而使得该“写操作”使用者获得一个该资源A的“私有”拷贝—资源B，可对资源B进行写操作。该“写操作”使用者对资源B的改变对于其他的使用者而言是不可见的，因为其他使用者看到的还是资源A。

要想在ucore中实现该机制，需要这样操作：

在每次`fork`进程时，不拷贝相应的内存空间，而是复制指针，并将其设为**只读**。这样一来，在只读取时，所有进程可以共享该部分，节省了资源。

而当一个进程需要进行写操作时，会导致`page_fault`，只需要在`page_fault`中对这种情况进行处理：复制这块内存空间，并修改。

## 练习3：阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现

### fork

创建并初始化进程，设置`proc`的各项信息，使进程能独立完成各种工作并响应系统操作。主要包括：分配资源，维护进程之间的关系，进程之间关系（父子、先后）的维护。

### exec

为进程设置入口，执行进程。主要通过调用`load_icode`读取新的ELF文件实现。

### wait

使进程进入等待状态。等待分为两种情况：1、等待指定的进程，2、等待其他任一进程执行，等待过程中，如果发现进程进入僵尸状态则会进行销毁。否则，令其父进程进入等待状态，递归执行。

### exit

进程的退出。内容包括销毁进程的各种信息和占用资源。

### 系统调用的实现

### 问题1：请分析fork/exec/wait/exit在实现中是如何影响进程的执行状态的？

通过修改进程的状态变量`state`，在调度中该变量就是进程的执行状态。

### 问题2：请给出ucore中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）

------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc, 
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:
                                            
  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+ 
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------
-----------------------------

## 与参考答案的区别

与参考答案几乎无区别，原因在于代码量较小且指示信息明确。

## 知识点总结

* 练习1：进程的创建与加载
* 练习2：进程的fork和空间复制
* 练习3：进程调度状态理解与分析
* 其他：用户线程

## 效果符合预期，如下

```
badsegment:              (1.8s)
  -check result:                             OK
  -check output:                             OK
divzero:                 (1.5s)
  -check result:                             OK
  -check output:                             OK
softint:                 (1.5s)
  -check result:                             OK
  -check output:                             OK
faultread:               (1.5s)
  -check result:                             OK
  -check output:                             OK
faultreadkernel:         (1.6s)
  -check result:                             OK
  -check output:                             OK
hello:                   (1.6s)
  -check result:                             OK
  -check output:                             OK
testbss:                 (1.6s)
  -check result:                             OK
  -check output:                             OK
pgdir:                   (1.5s)
  -check result:                             OK
  -check output:                             OK
yield:                   (1.5s)
  -check result:                             OK
  -check output:                             OK
badarg:                  (1.5s)
  -check result:                             OK
  -check output:                             OK
exit:                    (1.5s)
  -check result:                             OK
  -check output:                             OK
spin:                    (2.0s)
  -check result:                             OK
  -check output:                             OK
waitkill:                (1.7s)
  -check result:                             OK
  -check output:                             OK
forktest:                (1.5s)
  -check result:                             OK
  -check output:                             OK
forktree:                (1.5s)
  -check result:                             OK
  -check output:                             OK
Total Score: 150/150
```
