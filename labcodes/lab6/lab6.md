# LAB6 实验报告

## 练习0：填写已有实验

需要修改之前的代码，使其能顺利合并：

在`trap.c`的`trap_dispatch`中，增加
`sched_class_proc_tick(current);`
删除
`current->need_resched = 1;`。

在`proc.c`的`alloc_page`中，增加新的初始化内容：
```
		proc->rq = NULL;
		list_init(&(proc->run_link));
		proc->time_slice = 0;
		skew_heap_init(&(proc->lab6_run_pool));
		proc->lab6_stride = 0;
		proc->lab6_priority = 0;
```
分别表示：

* `rq`：运行队列；
* `run_link`：运行队列入口列表；
* `time_slice`：时间片长度；
* `lab6_run_pool`：运行进程池；
* `lab6_stride`：运行步长（用于调度）；
* `lab6_priority`：运行优先级。

## 练习1：使用 Round Robin 调度算法

### 问题1：请理解并分析sched_calss中各个函数指针的用法，并结合Round Robin 调度算法描述ucore的调度执行过程

RR算法中名主要实现的调度函数大致有以下五个：

* `init`：初始化调度器，得到初始调度进度；
* `enqueue`：将调度结束的函数加入等待队列，等待下一次调度；
* `dequeue`：将指定的函数从等待队列中删除；
* `pick_next`：选择下一个应该被换出的进程；
* `proc_tick`：时钟中断后更新调度信息。

在进程调度过程中，首先会使用`enqueue`函数将调出的函数加入等待队列，然后使用`pick_next`和`dequeue`函数，利用RR中的调度策略，选出下一个要调度执行的进程。而在时钟中断时，会调用`proc_tick`函数，对当前进程剩余时间**减一**，时间用完则调出，等待下一个时间片的分配。

此外，还需要注意，进程可以主动放弃剩余的时间片，如主动退出、进入等待等。

### 问题2：如何设计实现”多级反馈队列调度算法“，给出概要设计，鼓励给出详细设计

在多级反馈队列算法中，进程的优先级有所不同，调度策略也就有所不同，应当根据优先级不同，分别设立调度队列。

每次在需要调度进程时，先根据一定的规则选择一个调度队列，再在这个队列中选择进程调度。

## 练习2：实现 Stride Scheduling 调度算法

### 算法原理

在这个算法中，各进程具有不同的优先级，所能分配到的时间片段长度和优先级相关。

具体来说，在选择进程时，会选择`stride`值最小的进程进行执行。

并将该进程的`stride`值加上`BIG_STRIDE/(max(p->lab6_priority,1))`。

这样一来，在一段时间内，由于`BIG_STRIDE`足够大，进程分配到的时间片会和`BIG_STRIDE/(max(p->lab6_priority,1))`成正比，即优先级更高的的进程能分到更多的时间片。

### 算法实现

在文件夹下，已经给出了`default_sched_stride_c`文件，先覆盖掉`default_sched.c`文件。

在该文件中，有以下几处需要实现：

设定大数：

`#define BIG_STRIDE 0xfffffff`

在初始化时初始化进程列表、进程池和进程数量：

```
	list_init(&(rq->run_list));
	rq->lab6_run_pool = NULL;
	rq->proc_num = 0;
```

实现`enqueue`函数，把进程加入到调度队列中，更新进程池；重置进程的可用时间片长度，预备下一次调用；设置进程的运行队列入口；更新rq中的进程数量：

```
	rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool,&(proc->lab6_run_pool),proc_stride_comp_f);
	//proc->time_slice = rq->max_time_slice;
	if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
          proc->time_slice = rq->max_time_slice;
     }
	proc->rq = rq;
	rq->proc_num++;
```

实现`dequeue`函数，将进程队列中`stride`值最小的进程调出；更新进程数量：

```
	rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool,&(proc->lab6_run_pool),proc_stride_comp_f);
    rq->proc_num--;
```

在`pick_next`函数中，选择优先级最小的进程，更新`stride`：

```
	if (rq->lab6_run_pool == NULL)
		return NULL;
	struct proc_struct* p = le2proc(rq->lab6_run_pool,lab6_run_pool);
	p->lab6_stride += BIG_STRIDE/(max(p->lab6_priority,1));
	return p;
```

在`proc_tick`函数，修改当前进程的可用时间片，如果其被改为0，则说明需要调度了：

```
	if (proc->time_slice > 0)
		proc->time_slice--;
	if (proc->time_slice==0)
		proc->need_resched = 1;
```

### 特别注意

`proc_ticks`会影响程序的运行结果，在注释掉该部分后，程序的某些偶发错误消失了。

## 与参考答案的对比

与参考答案几乎无区别，原因在于每段代码量较小且指示信息明确。

## 知识点总结

* 实验1：RR调度算法与调度的基本概念
* 实验2：SS调度算法的实现
* 其他：实时调度、多处理器调度、优先级反置、FCFS等其他调度算法
