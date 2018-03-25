# LAB3 实验报告

## 练习0：填写已有实验

合并成功，无异常现象。

## 练习1：给未被映射的地址映射上物理页

该练习实际上是要求实现一个从虚拟地址到物理地址的映射，实现位置位于函数`do_pgfault`中。该函数在内存缺页的时候会被调用到，即所访问的虚拟地址在物理内存中未映射。

在调用该函数时，有两种可能的情况：

* 非法访问，访问了不存在的或是没有权限访问的内存地址，此时应当报错并处理，这段代码已经在框架中被实现。
* 合法访问，即缺页的情况。

在缺页中，又可以分为两种情况：

* 必然缺失，即该页是第一次被访问，在页表内尚未被分配，需要在本函数内分配；
* 冲突缺失，即因为操作系统调度的原因，这个页被`swap_out`了，需要重新调入。

练习1要处理的是必然缺失的情况，这时，只需要调用`pgdir_alloc_page(mm->pgdir,addr,perm)`来分配一个新的页即可，该函数会为它映射一个对应的物理内存。

### 问题1：请描述页目录项（Pag Director Entry）和页表（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。

在PDE和PTE中，`Access`属性与`Dirty`属性描述了这个页的使用情况和修改清空。当我们要把一个页交换到外部时，需要检查这个页的这两个属性，来决定交换时是否需要进行特定的操作。许多不必要的操作将会被节省，有效提高了整体效率。

### 问题2：如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

在缺页处理时，又发生了缺页，这意味着操作系统本身可能出现了问题。缺页处理时需要访问的内存是不应该缺失的。

在Qemu中，嵌套出现的缺页会导致模拟器退出。

查阅资料得知，如果在x86中发生了这种情况，会保存现场并进入`double fault`，以便维护人员获取错误信息。

### 实现代码


```
/*LAB3 EXERCISE 1: 2015011371*/
    ptep = get_pte(mm->pgdir,addr,1);              //(1) try to find a pte, if pte's PT(Page Table) isn't existed, then create a PT.
	cprintf("WWWWWWWWWWWW\t%x\n",*ptep);
    if (*ptep == 0) {
        pgdir_alloc_page(mm->pgdir,addr,perm);     //(2) if the phy addr isn't exist, then alloc a page & map the phy addr with logical addr
    }
    else {
    /*LAB3 EXERCISE 2: 2015011371
    * Now we think this pte is a  swap entry, we should load data from disk to a page with phy addr,
    * and map the phy addr with logical addr, trigger swap manager to record the access situation of this page.
    *
    *  Some Useful MACROs and DEFINEs, you can use them in below implementation.
    *  MACROs or Functions:
    *    swap_in(mm, addr, &page) : alloc a memory page, then according to the swap entry in PTE for addr,
    *                               find the addr of disk page, read the content of disk page into this memroy page
    *    page_insert ： build the map of phy addr of an Page with the linear addr la
    *    swap_map_swappable ： set the page swappable
    */
        if(swap_init_ok) {
            struct Page *page=NULL;
            swap_in(mm,addr,&page);                        //(1)According to the mm AND addr, try to load the content of right disk page
                                                          //    into the memory which page managed.
            page_insert(mm->pgdir,page,addr,perm);        //(2) According to the mm, addr AND page, setup the map of phy addr <---> logical addr
			swap_map_swappable(mm,addr,page,1);           //(3) make the page swappable.
			page->pra_vaddr = addr;//
        }
        else {
            cprintf("no swap_init_ok but ptep is %x, failed\n",*ptep);
            goto failed;
        }
   }
```

## 练习2：补充完成基于FIFO的页面替换算法

本练习实际上要解决的问题是在缺页时出现的`冲突缺失`，即需要的页面之前被`swap_out`了，这时需要在内存中找到最早进入内存的且可以被换出的页，然后将其换出。

在代码中，需要实现的核心函数包括`_fifo_map_swappable`、`_fifo_swap_out_victim`。

### 问题1：如果要在ucore上实现"extended clock页替换算法"请给你的设计方案，现有的swap_manager框架是否足以支持在ucore中实现此算法？如果是，请给你的设计方案。如果不是，请给出你的新的扩展和基此扩展的设计方案。并需要回答如下问题

* 需要被换出的页的特征是什么？

`Access`位和`Dirty`位均为0。

* 在ucore中如何判断具有这样特征的页？

只要找到对应页的PTE即可。

* 何时进行换入和换出操作？

在缺页异常发生，即物理内存中不存在需要查找的页时，需要执行换入操作。

在执行换入操作时，若发现物理内存已满，则需要执行换出操作。


## 与参考答案的区别

在部分地方，参考答案有更多的对边界条件和非法条件的判定，鲁棒性更好，更精细。

## 知识点总结

* 练习1：虚拟内存与物理内存的映射，虚拟存储技术。

* 练习2：页的换入换出，FIFO算法。

* 其他知识点：LRU算法，工作集置换及全局置换算法等。
