# LAB8 实验报告

## 练习0：填写已有实验

在`trap.c`中，对于`trap_dispatch`中，修改如下，修改原因见注释：

```
	case IRQ_OFFSET + IRQ_COM1:
    case IRQ_OFFSET + IRQ_KBD:
        // There are user level shell in LAB8, so we need change COM/KBD interrupt processing.
        c = cons_getc();
        {
          extern void dev_stdin_write(char c);
          dev_stdin_write(c);
        }
        break;
```


## 练习1：完成读文件操作的实现

修改`sfs_inode.c`代码中`sfs_io_nolock`函数，实现文件的读写。

根据注释，该函数实现了将指定文件块中的内容移动到对应的内存中的操作。主要通过调用以下三个函数实现：

* `sfs_bmap_load_nolock`：通过文件数据块编号计算出硬盘空间数据块的编号
* `sfs_buf_op`：对不完整的块进行数据复制操作
* `sfs_block_op`：对完整的块进行数据复制操作

在读取数据中，用户数据并非存在完整的若干个块中，我们需要支持一些边界情况的处理：

最一般的情况，文件占用了若干个块，且头尾都不完整：

|<----block001----->|<----block002----->|<----block003----->|<----block004----->|

             |<---------------file1------------------------------>|

一种特殊情况，文件占用了不到一个块：

|<----block101----->|<----block102----->|<----block103----->|<----block104----->|

                         |<--file2-->|

函数核心部分具体的实现情况如下：

```
	//LAB8:EXERCISE1 2015011371 HINT: call sfs_bmap_load_nolock, sfs_rbuf, sfs_rblock,etc. read different kind of blocks in file
	blkoff = offset%SFS_BLKSIZE;
	size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset);
	ret = sfs_bmap_load_nolock(sfs,sin,blkno,&ino);
	if(ret)
		goto out;
	ret = sfs_buf_op(sfs,buf,size,ino,blkoff);
	if(ret)
		goto out;
	alen+=size;
	if(!nblks)
		goto out;
	--nblks;
	++blkno;
	buf+=size;
	while(nblks)
	{
		ret = sfs_bmap_load_nolock(sfs,sin,blkno,&ino);
		if(ret)
			goto out;
		ret  = sfs_block_op(sfs,buf,blkno,1);
		if(ret)
			goto out;
		alen+=SFS_BLKSIZE;
		--nblks;
		++blkno;
		buf+=SFS_BLKSIZE;
	}
	size = endpos % SFS_BLKSIZE;
	if(size)
	{
		ret = sfs_bmap_load_nolock(sfs,sin,blkno,&ino);
		if(ret)
			goto out;
		ret = sfs_buf_op(sfs,buf,size,ino,0);
		if(ret)
			goto out;
		alen+=size;
		--nblks;
		++blkno;
		buf+=size;
	}
```

可以看出，这里分为三个大段，分别处理：文件头部不完整的块、文件中间完整的多块，文件末尾不完整的块。

小文件（头尾在同一个块内）的情况在第一种处理中得到解决。

每次读取一个块时，操作步骤如下：

* 获取这个块的编号；
* 如果这个块在文件头尾，则调用`sfs_buf_op`读取，否则调用`sfs_block_op`读取；
* 读取后维护相关变量（剩余块的数目、已经读取的长度等）。

### 问题1：设计实现”UNIX的PIPE机制“的概要设方案，鼓励给出详细设计方案

在UNIX系统中，PIPE机制提供了将任何东西视为一个文件的可能。

管道是一个固定大小的缓冲区域，当两个程序连到同一个管道时，一个程序负责向该区域写入数据，另一个程序则从这里读取速度。实现了程序数据的交互功能。

在实现时，应该这样设计：

* 创建，在管道创建时，为每个管道开辟一块专用的内存区域，同时把对管道的读写操作指向这个区域；
* 区域的大小可以根据情况设定，在Linux系统中为4kb；
* 两个程序内，分别把这块区域视为一个文件进行读写；
* 值得注意的是，当需要读而这块区域为空时，就应当产生阻塞，进程进入等待状态；
* 同理，当需要写而这块区域已满时，也需要阻塞；
* 考虑到区域大小有限，且两个程序一读一写，读取后数据就没有再存在管道中的必要性，有三种设计方案

方法0：互斥锁法

* 将这个区域加锁，让读写互斥即可。

缺点在于区域可能较小，要反复操作才能传递数据，效果不理想。

方案1：分块

* 将这个区域划分为两个部分，初始时，两个部分都标记为“空的”；
* 写进程可以写一个“空的”部分，当一个部分被写满时，将这个部分标记为“满的”，写进程可以写另一个部分；
* 读进程可以读一个部分，当且仅当这个部分是“满的”，在读取完毕后，这个部分标记为“空的”，读进程可以读另一个部分；
* 在读进程读取一个部分后，这个部分就被标记为“空的”。

这个方法存在的问题在于，如果只写入了不满2kb的数据，这个块不会被标记为“满的”，也就无法读取。

方案2：循环队列

* 对两个进程维护两个指针，分别表示“读到哪里了”、“写到哪里了”；
* 如果“写指针”在“读指针”后面，则意味着有内容可读；
* 如果“写指针”和“读指针”在同一位置，则意味没有内容可读；
* 如果“写指针”在“读指针”（循环意义下的）前一个位置，则意味已经写满；

通过这个方法，可以高效地同步读写这块区域，问题得到解决，对于两个指针，都只有一个程序能修改它的值，不会出现“写-写冲突”，而“读-写冲突”的最坏结果仅仅是失去一次读的机会。

## 练习2：完成基于文件系统的执行程序机制的实现

完成该练习需要修改`proc.c`文件；

其中，需要维护`proc_struct`中的新变量`filesp`，该变量是为了标明文件关系信息所用的。

该变量需要在`alloc_page`中初始化为`NULL`。

`proc->filesp = NULL;`

而在`do_fork`时，则需要进行`copy_file`操作将`current`中的`filesp`复制到新的`proc`中。

`copy_files(clone_flags,proc);`

主要需要修改的部分是`load_icode`，该函数从调用参数上就有较大改变。主要有以下几个方面：

* 不再从内存读取ELF文件，而是调用read从硬盘读取；
* 修改了调用参数，便于接收命令行操作；
* 调用参数的传递也需要随之修改，直接将命令行传入的参数压入栈中即可。

读取文件变化部分如下：

```
	//(3) copy TEXT/DATA section, build BSS parts in binary to memory space of process
    struct Page *page;
    //(3.1) get the file header of the bianry program (ELF format)
	struct elfhdr elfhdr;
    struct elfhdr *elf = &elfhdr;
	load_icode_read(fd, (void*)elf, sizeof(struct elfhdr), 0);
    //(3.2) get the entry of the program section headers of the bianry program (ELF format)
    struct proghdr phhdr;
    struct proghdr *ph = &phhdr;
    //(3.3) This program is valid?
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    uint32_t vm_flags, perm;
    //struct proghdr *ph_end = ph + elf->e_phnum;
    for (int i=0;i<elf->e_phnum;++i) {
		off_t phoff = elf->e_phoff + i * sizeof(struct proghdr);
		load_icode_read(fd,ph,sizeof(struct proghdr),phoff);
```

参数处理实现代码如下：

```
	//(6) setup uargc and uargv in user stacks
	int *tmp=USTACKTOP;
    uintptr_t *argv=(uintptr_t *)kmalloc(sizeof(uintptr_t)*argc);
	for(int i=argc-1;~i;--i)
	{
		int siz = strnlen(kargv[i],EXEC_MAX_ARG_LEN+1)+1;
		tmp-=siz;
		argv[i]=tmp;
		strcpy(tmp,kargv[i]);
	}
    for (int i=argc-1;~i;--i)
	{
    	tmp -= sizeof(uintptr_t);
    	*((int*)tmp) = argv[i];
    }
	kfree(argv);
    tmp -= sizeof(uint32_t);
    *(int *)tmp = argc;
```



### 问题1：设计实现基于”UNIX的硬链接和软链接机制“的概要设方案，鼓励给出详细设计方案

## 与参考答案的对比

与参考答案本质一样，实现细节略有不同。

## 知识点总结

* 实验1：文件的存储，以块为单位读写文件
* 实验2：文件的读取，文件描述符
* 其他：RAID[0-x]系统、文件系统权限控制

## 效果符合预期，节选如下

```
Iter 4, No.3 philosopher_condvar is eating
cond_signal end: cvp c04bca1c, cvp->count 0, cvp->owner->next_count 0
No.2 philosopher_condvar quit
Iter 2, No.4 philosopher_sema is thinking
No.1 philosopher_condvar quit
No.3 philosopher_condvar quit
Iter 2, No.4 philosopher_sema is eating
Iter 3, No.4 philosopher_sema is thinking
Iter 3, No.4 philosopher_sema is eating
Iter 4, No.4 philosopher_sema is thinking
Iter 4, No.4 philosopher_sema is eating
No.4 philosopher_sema quit
ls
 @ is  [directory] 2(hlinks) 23(blocks) 5888(bytes) : @'.'
   [d]   2(h)       23(b)     5888(s)   .
   [d]   2(h)       23(b)     5888(s)   ..
   [-]   1(h)       10(b)    40200(s)   yield
   [-]   1(h)       10(b)    40292(s)   priority
   [-]   1(h)       10(b)    40304(s)   matrix
   [-]   1(h)       10(b)    40220(s)   sleep
   [-]   1(h)       10(b)    40228(s)   forktest
   [-]   1(h)       10(b)    40204(s)   sleepkill
   [-]   1(h)       10(b)    40208(s)   faultreadkernel
   [-]   1(h)       10(b)    40224(s)   exit
   [-]   1(h)       10(b)    40252(s)   forktree
   [-]   1(h)       10(b)    40200(s)   softint
   [-]   1(h)       10(b)    40332(s)   waitkill
   [-]   1(h)       10(b)    40192(s)   pgdir
   [-]   1(h)       10(b)    40204(s)   faultread
   [-]   1(h)       10(b)    40360(s)   ls
   [-]   1(h)       10(b)    40224(s)   testbss
   [-]   1(h)       10(b)    40196(s)   spin
   [-]   1(h)       10(b)    40204(s)   badsegment
   [-]   1(h)       10(b)    40220(s)   divzero
   [-]   1(h)       10(b)    40200(s)   hello
   [-]   1(h)       11(b)    44508(s)   sh
   [-]   1(h)       10(b)    40200(s)   badarg
lsdir: step 4
$ hello
Hello world!!.
I am process 14.
hello pass.
$ forktree
forktree process will sleep 400 ticks
000f: I am ''
0011: I am '1'
0013: I am '11'
0015: I am '111'
0017: I am '1111'
0016: I am '1110'
0014: I am '110'
0019: I am '1101'
0018: I am '1100'
0012: I am '10'
001b: I am '101'
001d: I am '1011'
001c: I am '1010'
```
