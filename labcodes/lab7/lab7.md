# LAB7 实验报告

## 练习0：填写已有实验

需要修改之前的代码，使其能顺利合并：

`proc.h`中，修改最大进程名称长度为50。

在`trap.c`的`trap_dispatch`中，增加`run_timer_list`。

在`vmm.c`的`mm_create`中，将`lock_init`替换为`sem_init`。

## 练习1：理解内核级信号量的实现和基于内核级信号量的哲学家就餐问题

### 练习1.1：内核级信号量的设计描述，并说明其大致执行流流程

先分析内核级信号量的测试过程：

* 执行`init`，创建用户程序空间。
* 执行`check_sync`函数，开始测试，测试分为两个部分，前一半测试即是这个部分用的。

在测试中，先使用`kernel_execve`生成了n个“哲学家”（内核线程）。

所有哲学家共享了一个临界区信号量`mutex`，用来保证操作`state_sema`数组时不会发生冲突。

通过这个信号量，我们完成了对该数组加锁操作。

对于每个哲学家，还有一个信号量`s[i]`，表示这个哲学家的进餐状态。

当一个哲学家想要进餐时（进程想要执行时），哲学家会索取两个叉子，如果不能取得，则`s[i]`进入阻塞状态（HUNGRY）。

当一个哲学家放下叉子后，他会询问他两侧的哲学家是否需要叉子（HUNGRY），如果是，且该哲学家恰好在这时发现了两把空闲的叉子，则他会进入进餐状态。

在程序实现上，`up`和`down`函数表示了对临界区的控制，分别为离开临界区和进入临界区。

以下是详细代码：

```
static __noinline void __up(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        wait_t *wait;
        if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL) {
            sem->value ++;
        }
        else {
            assert(wait->proc->wait_state == wait_state);
            wakeup_wait(&(sem->wait_queue), wait, wait_state, 1);
        }
    }
    local_intr_restore(intr_flag);
}

static __noinline uint32_t __down(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    local_intr_save(intr_flag);
    if (sem->value > 0) {
        sem->value --;
        local_intr_restore(intr_flag);
        return 0;
    }
    wait_t __wait, *wait = &__wait;
    wait_current_set(&(sem->wait_queue), wait, wait_state);
    local_intr_restore(intr_flag);

    schedule();

    local_intr_save(intr_flag);
    wait_current_del(&(sem->wait_queue), wait);
    local_intr_restore(intr_flag);

    if (wait->wakeup_flags != wait_state) {
        return wait->wakeup_flags;
    }
    return 0;
}

void
up(semaphore_t *sem) {
    __up(sem, WT_KSEM);
}

void
down(semaphore_t *sem) {
    uint32_t flags = __down(sem, WT_KSEM);
    assert(flags == 0);
}

bool
try_down(semaphore_t *sem) {
    bool intr_flag, ret = 0;
    local_intr_save(intr_flag);
    if (sem->value > 0) {
        sem->value --, ret = 1;
    }
    local_intr_restore(intr_flag);
    return ret;
}
```

### 练习1.2：用户态进程/线程提供信号量机制的设计方案，并比较说明给内核级提供信号量机制的异同。

在内核态中已经实现了这个功能，那么，要迁移到用户态，只需要提供合适的借口即可。

因此，需要在`proc_struct`中增加一个列表，用于维护所有的信号量，接口如下：

* 新建，用于创建一个信号量，并加入到列表中。
* up(i)，调用指定的内核函数。
* down(i)，调用指定的内核函数。

关于内核中信号量的操作，可以通过新增一系列系统调用实现。

这些操作实际上都实现在内核态，因此原本具有的原子操作等属性也都会具备，可以被正常使用。

## 练习2：完成内核级条件变量和基于内核级条件变量的哲学家就餐问题

### 练习2.1：内核级条件变量的设计描述，并说其大致执行流流程

内核态采用Hoare管程实现。

进入管程时，需要调用`down`函数。

退出时，需要调用`up`函数。

在管程中，运用同步互斥来控制，结构体实现在`condvar`与`monitor`中，具体如下：

```
typedef struct condvar{
    semaphore_t sem;        // the sem semaphore  is used to down the waiting proc, and the signaling proc should up the waiting proc
    int count;              // the number of waiters on condvar
    monitor_t * owner;      // the owner(monitor) of this condvar
} condvar_t;

typedef struct monitor{
    semaphore_t mutex;      // the mutex lock for going into the routines in monitor, should be initialized to 1
    semaphore_t next;       // the next semaphore is used to down the signaling proc itself, and the other OR wakeuped waiting proc should wake up the sleeped signaling proc.
    int next_count;         // the number of of sleeped signaling proc
    condvar_t *cv;          // the condvars in monitor
} monitor_t;
```

在`phi_take_forks_condvar`中，在哲学家希望取得叉子时，设置其为HUNGRY，调用`phi_test_condvar`函数，该函数会测试这个哲学家是否应该进入吃饭状态，如果是，则执行相应操作。如果他没有进入吃饭状态，则设置该信号量加入等待队列，等待时机吃饭。

在`phi_put_forks_condvar`中，在哲学家放下叉子时，设置其为THINKING，调用`phi_test_condvar`函数，该函数会测试这个哲学家左右两侧的哲学家是否应该进入吃饭状态，如果是，则执行相应操作。

```
void phi_take_forks_condvar(int i) {
     down(&(mtp->mutex));
//--------into routine in monitor--------------
     // LAB7 EXERCISE1: 2015011371
     // I am hungry
	state_condvar[i]=HUNGRY;
     // try to get fork
	phi_test_condvar(i);
	while(state_condvar[i]!=EATING)
		cond_wait(&(mtp->cv[i]));
//--------leave routine in monitor--------------
      if(mtp->next_count>0)
         up(&(mtp->next));
      else
         up(&(mtp->mutex));
}

void phi_put_forks_condvar(int i) {
     down(&(mtp->mutex));
//--------into routine in monitor--------------
     // LAB7 EXERCISE1: 2015011371
     // I ate over
	state_condvar[i]=THINKING;
     // test left and right neighbors
	phi_test_condvar(LEFT);
	phi_test_condvar(RIGHT);
//--------leave routine in monitor--------------
     if(mtp->next_count>0)
        up(&(mtp->next));
     else
        up(&(mtp->mutex));
}
```

在`monitor.c`中，则需要实现`cond_signal`和`cond_wait`函数，它们用于两种不同情况下阻塞一个convar_t变量。

在`cond_wait`中，由于缺少资源必须阻塞，先将等待队列长度+1；然后判断，是否存在一个next优先的进程，如果存在，则将其从临界态释放。否则，将之前阻塞的部分让出，阻塞自己。以让出管程，等待资源。

在`cond_signal`中，是让等待的进程更快地执行，需要先观察是否有等待队列，如果有，则需要执行等待队列中的进程，以便在之后保证时序地执行自己。

实现代码如下：

```
// Unlock one of threads waiting on the condition variable. 
void 
cond_signal (condvar_t *cvp) {
   //LAB7 EXERCISE1: 2015011371
	cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);  
	if(cvp->count>0)
	{
		cvp->owner->next_count++;
		up(&(cvp->sem));
		down(&(cvp->owner->next));
		cvp->owner->next_count--;
	}
  /*
   *      cond_signal(cv) {
   *          if(cv.count>0) {
   *             mt.next_count ++;
   *             signal(cv.sem);
   *             wait(mt.next);
   *             mt.next_count--;
   *          }
   *       }
   */
   cprintf("cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

// Suspend calling thread on a condition variable waiting for condition Atomically unlocks 
// mutex and suspends calling thread on conditional variable after waking up locks mutex. Notice: mp is mutex semaphore for monitor's procedures
void
cond_wait (condvar_t *cvp) {
    //LAB7 EXERCISE1: 2015011371
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
	cvp->count++;
	if(cvp->owner->next_count>0)
		up(&(cvp->owner->next));
	else
		up(&(cvp->owner->mutex));
	down(&(cvp->sem));
	cvp->count--;
   /*
    *         cv.count ++;
    *         if(mt.next_count>0)
    *            signal(mt.next)
    *         else
    *            signal(mt.mutex);
    *         wait(cv.sem);
    *         cv.count --;
    */
    cprintf("cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
```

### 练习2.2：内核级条件变量的设计描述，并说其大致执行流流程

和练习1相似，用户态中的条件变量的设计。只需要提供相应的系统调用，来使用内核态的变量即可。

### 练习2.3：能否不用基于信号量机制来完成条件变量？如果不能，请给出理由，如果能，请给出设计说明和具体实现。

我认为可以，因为还能使用原子操作来进行条件变量的维护，达到信号量想要达到的效果。

## 和参考答案对比

本质相同，因为每一个地方都提供了充足的注释，实现起来就完全按照提示。

##知识点总结

* 实验1：哲学家就餐问题、信号量、同步互斥锁。
* 实验2：管程和条件变量，同步互斥。
* 其他：原子指令实现同步虎互斥、生产者-消费者问题。
