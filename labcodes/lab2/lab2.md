# LAB2 实验报告

## 练习0：填写已有实验

合并成功，无异常现象。

## 练习1：实现 first-fit 连续物理内存分配算法

### 程序设计与实现思想

实际上，在Lab2中已经给出了first-fit算法的一个实现。但这个实现是不完善的，在`default_check`函数中会抛出`assert`异常。

结合我对该算法的理解，仔细阅读源代码，分析调试后发现：原有代码中的实现是不对的，根据算法，first-fit算法，`free_list`应当按照从低到高的顺序维护了可用的地址空间。而原有代码的实现中，却没有维护这方面的信息，而是随意地将可用地址加入了`free_list`。

这种做法虽然能够让这块代码正常运行，但是运行过程中会生成很多不必要的内存碎片，使内存总体利用率下降。

我对这个算法进行了修改，使得它能满足要求，顺利通过`assert`，达到设想中的效果。

这个算法实现在`kern/mm/default_pmm.c`中，具体改动有以下几处：

```
		#ifndef BUBBY
        if (page->property > n) {
            struct Page *p = page + n;
		    p->flags = p->property = 0;
		    set_page_ref(p, 0);
            p->property = page->property - n;
			SetPageProperty(p);//
		    list_add(&(page->page_link), &(p->page_link));//
			//cprintf("pro %d\n",p->property);
			//cprintf("nex %d\n",list_next(&free_list));
			//cprintf("p   %d %d\n",&p,p->page_link);
    	}
		list_del(&(page->page_link));//
		#endif
```

上面这段代码修改了申请内存时剩余空间加回`free_list`时的插入位置，即找到序列中正确的位置再加入。

```
	#ifndef BUBBY
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list) {
        p = le2page(le, page_link);
        le = list_next(le);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
        else if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
    }
	#endif
```

这段代码则在内存释放时使用，它除了能够支持放入的内存块在正确的位置插入外，还监测了其是否和它的前一个、后一个块连续，如果连续则会把它们连成一块连续的内存空间再插入，方便后续代码申请更大的连续内存空间。

### 问题：你的first fit算法是否有进一步的改进空间

在上述工作中，第二部分中的**合并连续的空闲内存**实际上就是first-fit算法的一个改进，它与原算法相比，有利于获得更连续的内存空间进行再分配，是绝对占优的。

同时，在对比参考答案时，我发现，参考答案的实现中，每个空闲页都在`free_list`中被记录下来，事实上这是不必要的，我们只需要对每个空闲的连续段，记录其头部位置及长度，即可完成内存分配的各种任务。同时，我这种设计能够比标准答案更快地找到空闲页，效率上有明显的优化。

结合上一段，在页属性中，我们只管理了会被`free_list`关注到的页的`property`位，对其他页的这一属性不进行研究，这样的设计同样是为了优化效率、简化程序考虑的。

在时间效率上，我优化后的算法效率至少要比原有算法高一倍，在实际使用中往往能高出数十倍以上，非常可观。

上述几点都已经在程序中有了**具体的实现**，以下是可能的改进方向：

* 对扫描顺序进行调整：在程序运行了一段时间后，内存碎片化往往比较严重，在低地址的区域有很多碎片。分配内存时扫描这些碎片会消耗大量的时间。可以考虑不从低地址开始，而是从高地址开始向低地址扫描，或是从上次找到内存的下一块开始扫描。此举能大大节约扫描时间。
* 延迟插入算法：内存在被释放后就被插入`free_list`，这浪费了大量的时间来完成插入操作，可能可以考虑改进插入时机。在内存被释放时先不插入，而是在系统空闲或在某次请求内存失败时再尝试。

## 练习2：实现寻找虚拟地址对应的页表项

### 代码实现

寻找虚拟地址大致要分以下几个步骤进行：

* 找到目录表的头
* 如果目录表头存在，则在页表中，根据页地址找到其虚拟位置
* 如果是不存在，且要求新建地址，则新建该项，新建时注意以下几点即可
* 建立相关数据记录
* 建立链接
* 设置信息
* 最后要返回相关信息

```
	pde_t *pdep = &pgdir[PDX(la)];
	// (1) find page directory entry
	//cprintf("GET_PTE %x\n",*pdep);
    if (!(PTE_P & *pdep)) {// (2) check if entry is not present
		if (!create)
			return NULL;
		struct Page* p = alloc_page();
		if (p == NULL)
			return NULL;
		// (3) check if creating is needed, then alloc page for page table
        // CAUTION: this page is used for page table, not for common data page
        set_page_ref(p,1);         // (4) set page reference
        uintptr_t pa = page2pa(p); // (5) get linear address of page
        memset(KADDR(pa),0,PGSIZE);// (6) clear page content using memset
        *pdep = pa|7;              // (7) set page directory entry's permission
	}
	return &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)];
```

### 问题1：请描述页目录项（Pag Director Entry）和页表（Page Table Entry）中每个组成部分的含义和以及对ucore而言的潜在用处。

PDE和PTE从名称上就可以知道其大致作用，相当于在一本百科全书上，PDE是其总目录，PTE则是每个专业的目录表。

因为都是目录，二者的存储结构也有一定的相似之处，但也有许多不同。

PDE的各部分组成为：

```
|<------ 31~12------>|<------ 11~0 --------->| 比特
                     |b a 9 8 7 6 5 4 3 2 1 0| 
|--------------------|-|-|-|-|-|-|-|-|-|-|-|-| 占位
|<-------index------>| AVL |G|P|0|A|P|P|U|R|P| 属性
                             |S|   |C|W|/|/|
                                   |D|T|S|W|
```

PTE的组成为：

```
|<------ 31~12------>|<------ 11~0 --------->| 比特
                     |b a 9 8 7 6 5 4 3 2 1 0|
|--------------------|-|-|-|-|-|-|-|-|-|-|-|-| 占位
|<-------index------>| AVL |G|P|D|A|P|P|U|R|P| 属性
                             |A|   |C|W|/|/|
                             |T|   |D|T|S|W|
```

部分属性列举如下：

* P：有效位。0 表示当前表项无效。
* R/W: 0 表示只读。1表示可读写。
* U/S: 0 表示3特权级程序可访问，1表示只能0、1、2特权级可访问。
* A: 0 表示该页未被访问，1表示已被访问。
* D: 脏位。0表示该页未写过，1表示该页被写过。
* PS: 只存在于页目录表。0表示这是4KB页，指向一个页表。1表示这是4MB大页，直接指向物理页。

ucore可以利用这些属性，很方便地对PDE或PTE进行操作。

### 问题2：如果ucore执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

在访问内存时，会在mmu模块中确认内存是否存在，如果不存在，则触发一个Page Fault异常。

在异常发生时，会依照以下顺序处理：

* 保存现场，将异常发生时的状况进行保存；
* 记录下相应的错误信息；
* （如果在用户态）切换到内核态；
* 在idt表中，查找ISR地址，决定是否进入中断；
* 根据决定继续执行代码。

## 练习3：释放某虚地址所在的页并取消对应二级页表项的映射

要依次执行以下步骤：

* 确认页表是否存在
* 找到对应的页
* 减少该页的`ref`
* 当`ref=0`时清空该页
* 删除页入口
* 更行TLB

代码实现在`pmm.c`中，具体如下：

```
	if (PTE_P & *ptep) {                      //(1) check if this page table entry is present
        struct Page *page = pte2page(*ptep);  //(2) find corresponding page to pte
        page_ref_dec(page);                   //(3) decrease page reference
        if(page_ref(page)==0)
			free_page(page);                  //(4) and free this page when page reference reachs 0
        *ptep = 0;                            //(5) clear second page table entry
        tlb_invalidate(pgdir,la);             //(6) flush tlb
    }
```

### 问题1：数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

有关系，Page数组是用来管理内存的，数组中的Page实际上就对应到内存中的Page。页目录和页表都是存在内存中，页表中存储的是页的物理地址。而Page本身则被存在了内核代码段中。

### 问题2：如果希望虚拟地址与物理地址相等，则需要如何修改lab2，完成此事？ 

需要修改多处代码：

* 在bootloader阶段，无需改动，自然满足条件。
* 段机制映射阶段，修改gdt表项和**链接脚本**，以避免`KERNBASE`的影响，将起始地址减去`0xC0000000`。
* 页机制映射阶段，修改内容与段机制类似。

## 扩展练习 - BUBBY算法

实现了BUBBY算法，并添加到代码中，数据结构实现在`bubby.h`中，并在`default_pmm.c`中用宏来控制是否使用。

在这里，使用`segment tree`数据结构来维护空闲的内存块。

分配内存时规则如下：

* 先把内存大小改为2的整数次幂，便于查找相应大小的内存块。
* 在每个`node`上，记录其子树中最长的连续段（2的幂次段）的长度。
* 如果最长连续段小于需要的大小，显然无法分配，返回NULL。
* 先考察当前节点是否符合要求（长度恰好等于需要的长度）。
* 递归考察是否需要的左节点是否可以安排。
* 同样递归右节点。

释放内存时，从下到上递归释放，并更新节点上的`node`信息。

## 与参考答案的比对

在实验1中，使用了比参考答案更优的策略，减少了`free_list`中的表项，提高了效率。

在实验2、3中，与参考答案实现相似，无本质差别。

## 知识点总结

练习1：如何用First-Fit算法进行内存分配。

练习2、3：建立和释放虚拟地址的页表项映射。

## 其他重要的知识点

段机制。

更高级的内存分配方法与碎片整理方法。

## 效果符合预期，如下

```
Check PMM:               (2.4s)
  -check pmm:                                OK
  -check page table:                         OK
  -check ticks:                              OK
Total Score: 50/50
```
