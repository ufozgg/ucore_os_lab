# LAB3 实验报告

## 练习0：填写已有实验

合并成功，无异常现象。

相比于lab3，本实验新增了两个文件夹：`process`、`schedule`。

## 练习1：分配并初始化一个进程控制块

该练习较为简单，即完成`proc.c`中`alloc_page`时的初始化项目。

要被初始化的时一个 proc_struct* 。

其中的内容及其含义如下：

* state：进程状态
* pid：进程编号
* runs：运行次数
* kstack：虚拟地址
* need_resched：是否需要调度
* parent：父进程
* mm：虚拟内存管理器
* context：进程运行状态信息
* tf：中断指针，用于状态切换
* cr3：PDT基址
* flags：标志
* name：名字

### 问题1：请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用是啥？

`context`用于在进程切换时存储进程相应的信息，包括寄存器和程序指针等。

`tf`用于存储进入中断时的用户态相关信息，在用户态和内核切换时用到。

### 实现代码

```
		proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
```

## 练习2：为新创建的内核线程分配资源

该练习需要完成`proc.c`中`do_fork`时进行的资源分配工作。

实现该功能需要依次执行以下步骤：

* 执行`alloc_proc`初始化一个进程；
* 调用`setup_kstack`为其开辟子进程调用空间；
* 调用`copy_mm`获得内存空间；
* 设置进程ID；
* 将进程添加到进程列表里；
* 唤醒进程，开始工作；
* 返回进程ID。

### 问题1：请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由

在同一时刻是。

pid是通过get_pid函数进行的，阅读该函数代码可以发现，这个代码实在当前未被使用的pid中，选出一个来进行分配的。因此，同一时刻任何两个进程的pid都是互不相同的。

但也要注意，不同时刻，两个进程的pid可能会一样，这是由于一个进程销毁后，其占用的pid会被释放，可能会被再分配。

同时，该函数中，如果pid资源用完了，则会进入死锁，整个系统无法工作。因此，系统通过限制了`MAX_PROCESS`的数量来避免这种情况的发生。

### 实现代码

```
//LAB4:EXERCISE2 2015011371
	proc = alloc_proc();		//    1. call alloc_proc to allocate a proc_struct
	setup_kstack(proc);			//    2. call setup_kstack to allocate a kernel stack for child process
	copy_mm(clone_flags,proc);	//    3. call copy_mm to dup OR share mm according clone_flag
	copy_thread(proc,stack,tf);	//    4. call copy_thread to setup tf & context in proc_struct
	proc->pid = get_pid();
	hash_proc(proc);
	list_add(&proc_list,&(proc->list_link));
	++nr_process;				//    5. insert proc_struct into hash_list && proc_list
	wakeup_proc(proc);			//    6. call wakeup_proc to make the new child process RUNNABLE
	ret = proc->pid;			//    7. set ret vaule using child proc's pid
```

## 练习3：阅读代码，理解 proc_run 函数和它调用的函数如何完成进程切换的。

`proc_run`代码如下：

```
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = proc;
            load_esp0(next->kstack + KSTACKSIZE);
            lcr3(next->cr3);
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}
```

可以看出，这个函数先将`current`指向了`proc`，即修改为当前运行的进程；然后，利用`load_esp0`函数进行了内核栈顶指针的设置，以便在函数运行时从用户态切换到内核态；之后，通过`lrc3`更新页表项，使用当前进程的页表；最后执行`switch_to`，切换完成。

### 问题1：在本实验的执行过程中，创建且运行了几个内核线程？

观察到两个，一个是`init`，一个是`idle`。

### 问题2：语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);在这里有何作用?请说明理由

分析得到，该语句时为了屏蔽外部中断，保证这之间的代码是被连续执行的，避免引起进程混乱。

## 与参考答案的区别

与参考答案基本相同，但在部分地方，参考答案屏蔽了中断，而我的代码没有。在测试环境下，由于这段代码执行时没有中断，故也能正常运行。但在实用环境下还是应当屏蔽。

## 知识点总结

* 练习1：进程的结构信息
* 练习2：进程的创建与分配
* 练习3：进程切换
* 其他知识点：进程的等待、进程退出的完整实现

## 效果符合预期，如下

```
Check VMM:               (1.8s)
  -check pmm:                                OK
  -check page table:                         OK
  -check vmm:                                OK
  -check swap page fault:                    OK
  -check ticks:                              OK
  -check initproc:                           OK
Total Score: 90/90
```
