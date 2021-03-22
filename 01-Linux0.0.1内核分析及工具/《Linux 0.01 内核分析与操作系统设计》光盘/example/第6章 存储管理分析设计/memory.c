#include <signal.h>

#include <linux/config.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <asm/system.h>

int do_exit(long code);

#define invalidate() \
//下面的内嵌汇编的意思是：
//	movl	0	%eax
//	movl	%eax	%cr3		//将页目录的物理基地址设为0
__asm__("movl %%eax,%%cr3"::"a" (0))

#if (BUFFER_END < 0x100000)		//0x200000=2M
#define LOW_MEM 0x100000
#else
#define LOW_MEM BUFFER_END
#endif

/* these are not to be changed - thay are calculated from the above */
#define PAGING_MEMORY (HIGH_MEMORY - LOW_MEM)	//分页内存有6M
#define PAGING_PAGES (PAGING_MEMORY/4096)	//有多少个页=1536个页
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)		//页号

#if (PAGING_PAGES < 10)
#error "Won't work"
#endif

#define copy_page(from,to) \
//下面的内嵌汇编的意思是：
//	movl	1024	%ecx
//	movl	from	%esi
//	movl	to	%edi
//	cld
//	rep	movsl
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024):"cx","di","si")

static unsigned short mem_map [ PAGING_PAGES ] = {0,};	//如果某页号置1就说明此页已在用

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
unsigned long get_free_page(void)
{
    register unsigned long __res asm("ax");
    //下面的内嵌汇编的意思是：
    //	movl	0	%eax
    //	movl	LOW_MEM	%ebx
    //	movl	PAGING_PAGES	%ecx
    //	movl	mem_map+PAGING_PAGES-1	%edi
    //	std							//地址减量方向
    //	repne						//ZF=0时循环
    //	scasw						//eax---->es:di
    //	jne	1f						//每页都已用，直接返回
    //	movw	$1	%edi+2			//将mem_map[X]=1，其中的X是寻找到的第几个页
    //	sall	$12	%ecx				//PAGING_PAGES乘4096，计数器以字节为单位，寻找页的基址
    //	movl	%ecx	%edx		//保存基址
    //	addl	%ebx	%edx			//基址加BUFFER_END成为实际地址
    //	movl	$1024	%ecx
    //	leal	%edx+4092	%edi		//%edx+4092是因为前面是std，所以要减掉4
    //	rep
    //	stosl							//eax--->es:edi，主要是将该页初始化为0
    //	movl	%edx	%eax		//将此基址保存入eax
    //	movl	%eax	_res
    __asm__("std ; repne ; scasw\n\t"
            "jne 1f\n\t"
            "movw $1,2(%%edi)\n\t"
            "sall $12,%%ecx\n\t"
            "movl %%ecx,%%edx\n\t"
            "addl %2,%%edx\n\t"
            "movl $1024,%%ecx\n\t"
            "leal 4092(%%edx),%%edi\n\t"
            "rep ; stosl\n\t"
            "movl %%edx,%%eax\n"
            "1:"
        :"=a" (__res)
                    :"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
                    "D" (mem_map+PAGING_PAGES-1)
                    :"di","cx","dx");
    return __res;
}

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()'
 */
void free_page(unsigned long addr)
{
    if (addr<LOW_MEM) return;			//不能低于2M
    if (addr>HIGH_MEMORY)				//不能高于8M
        panic("trying to free nonexistent page");
    //下面2行还原此基址在mem_map[]的地址
    addr -= LOW_MEM;
    addr >>= 12;
    if (mem_map[addr]--) return;			//如果是共享内存，不FREE
    mem_map[addr]=0;					//FREE此页
    panic("trying to free free page");
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 */
int free_page_tables(unsigned long from,unsigned long size)
{
    unsigned long *pg_table;
    unsigned long * dir, nr;

    if (from & 0x3fffff)			//必须能被4M整除，最小数应为0x400000
        panic("free_page_tables called with wrong alignment");
    if (!from)					//不能为0，不能FREE内核区
        panic("Trying to free up swapper memory space");
    size = (size + 0x3fffff) >> 22;	//1代表4M，2代表8M，依此类推
    dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */	//找出目录地址，该地址是由copy_page_tables设置，其中from>>20是只移动20位，而不是22位，是因为以后要乘4寻址，其中0xffc是取32位地址的最高10位并乘4
    for ( ; size-->0 ; dir++) {
        if (!(1 & *dir))
            continue;
        pg_table = (unsigned long *) (0xfffff000 & *dir);	//从目录地址中取得页表地址
        for (nr=0 ; nr<1024 ; nr++) {
            if (1 & *pg_table)		//如果该页表在内存中，就FREE它
                free_page(0xfffff000 & *pg_table);
            *pg_table = 0;			//将此页表项的指针指的内容设为0
            pg_table++;			//指向下一个页表项
        }
        free_page(0xfffff000 & *dir);	//FREE此目录地址内容所指的内存地址
        *dir = 0;					//标记此目录空闲
    }
    invalidate();					//将cr3设为0
    return 0;
}

/*
 *  Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :-)
 *
 * Note! We don't copy just any chunks of memory - addresses have to
 * be divisible by 4Mb (one page-directory entry), as this makes the
 * function easier. It's used only by fork anyway.
 *
 * NOTE 2!! When from==0 we are copying kernel space for the first
 * fork(). Then we DONT want to copy a full page-directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it
 * doesn't take any more memory - we don't copy-on-write in the low
 * 1 Mb-range, so the pages can be shared with the kernel. Thus the
 * special case for nr=xxxx.
 */
int copy_page_tables(unsigned long from,unsigned long to,long size)
{
    unsigned long * from_page_table;
    unsigned long * to_page_table;
    unsigned long this_page;
    unsigned long * from_dir, * to_dir;
    unsigned long nr;

    if ((from&0x3fffff) || (to&0x3fffff))		//from和to必须被4M整除，4M边界对齐
        panic("copy_page_tables called with wrong alignment");
    from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */	　//取from高10位，然后乘4，形成目录项
    to_dir = (unsigned long *) ((to>>20) & 0xffc);
    size = ((unsigned) (size+0x3fffff)) >> 22;
    for( ; size-->0 ; from_dir++,to_dir++) {
        if (1 & *to_dir)		//看看to_dir指针指定的内存地址的值的最低位是否为1，如为1，说明该值所表示的地址已分配
            panic("copy_page_tables: already exist");
        if (!(1 & *from_dir))	//from_dir指针中的值表示的地址必须已被分配
            continue;
        from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
        if (!(to_page_table = (unsigned long *) get_free_page()))	//分配一个自由物理内存地址给它
            return -1;	/* Out of memory, see freeing */
        *to_dir = ((unsigned long) to_page_table) | 7;		//在to_dir所指的内存中填写刚取得的自由内存地址，并设该值的最低位为1，表示已分配
        nr = (from==0)?0xA0:1024;		//160X4K=640K
        for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
            this_page = *from_page_table;	　　//将源页表指针所指的值赋给this_page
            if (!(1 & this_page))
                continue;
            this_page &= ~2;		　　　　　	//将该值表示的地址的最低第2位置为0，只允许级别3的进程读
            *to_page_table = this_page;			//将源页表中的页表项值放入to_page_table指针所指的内存地址中
            if (this_page > LOW_MEM) {		//是否是内核区域
                *from_page_table = this_page;	//将源页表项的第2位也置为0，原因是共享了
                this_page -= LOW_MEM;		//将该页映射置1，表示在用
                this_page >>= 12;
                mem_map[this_page]++;
            }
        }
    }
    invalidate();								//将cr3代表的基址设为0
    return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 */
unsigned long put_page(unsigned long page,unsigned long address)
{
    unsigned long tmp, *page_table;

    /* NOTE !!! This uses the fact that _pg_dir=0 */

    if (page < LOW_MEM || page > HIGH_MEMORY)　	//不低于2M，不高于8M
        printk("Trying to put page %p at %p\n",page,address);
    if (mem_map[(page-LOW_MEM)>>12] != 1)		　　//此页是否在内存中
        printk("mem_map disagrees with %p at %p\n",page,address);
    page_table = (unsigned long *) ((address>>20) & 0xffc);　　//该指针应为页目录指针
    if ((*page_table)&1)					　　　　　　　//该页目录是否在内存中
        page_table = (unsigned long *) (0xfffff000 & *page_table);
    else {
        if (!(tmp=get_free_page()))		　　　　　　　　	//如果页目录不在内存中，找一个自由页，把该页的地址做为页表首指针
            return 0;
        *page_table = tmp|7;
        page_table = (unsigned long *) tmp;
    }
    page_table[(address>>12) & 0x3ff] = page | 7;	//该地址相对于page_table首地址的相对位置，在此处放入page|7
    return page;
}

void un_wp_page(unsigned long * table_entry)	//此函数的作用是保护内核区页或共享内存页
{
    unsigned long old_page,new_page;

    old_page = 0xfffff000 & *table_entry;
    if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {	//如果老页不在内核区并且页已分配在用
        *table_entry |= 2;				　　//只将此页的第2位置1，使该页可读可写
        return;					　　　　//直接返回
    }
    if (!(new_page=get_free_page()))
        do_exit(SIGSEGV);
    if (old_page >= LOW_MEM)
        mem_map[MAP_NR(old_page)]--;	//将共享页的计数减1
    *table_entry = new_page | 7;			　　//入口重新定向为新页的地址
    copy_page(old_page,new_page);			//将老页拷贝入新页
}

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 */
void do_wp_page(unsigned long error_code,unsigned long address)
{
    un_wp_page((unsigned long *)
               (((address>>10) & 0xffc) + (0xfffff000 &*((unsigned long *) ((address>>20) &0xffc)))));		　　　　　　　　　　　　　　　　//其中((address>>10) & 0xffc)是相对首地址的偏移地址，是取address的12到21位共10位，(0xfffff000 &*((unsigned long *) ((address>>20) &0xffc))))是计算出页表首地址

}

void write_verify(unsigned long address)
{
    unsigned long page;

    if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
        return;					　　//如果页目录指向的页表不在内存，直接返回
    page &= 0xfffff000;					//取页表首地址
    page += ((address>>10) & 0xffc);		//加上address中的中间10位，并乘4，就形成了页表项地址
    if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */	//对共享内存的处理
        un_wp_page((unsigned long *) page);
    return;
}

void do_no_page(unsigned long error_code,unsigned long address)	//如果没有页，取一个自由页，并放入指定的页表地址（address）中
{
    unsigned long tmp;

    if (tmp=get_free_page())
        if (put_page(tmp,address))
            return;
    do_exit(SIGSEGV);
}

void calc_mem(void)
{
    int i,j,k,free=0;
    long * pg_tbl;

    for(i=0 ; i<PAGING_PAGES ; i++)
        if (!mem_map[i]) free++;		　　　　　　　　　	//计算有多少自由页
    printk("%d pages free (of %d)\n\r",free,PAGING_PAGES);
    for(i=2 ; i<1024 ; i++) {			　　　　	//计算各页目录中的页表用掉了多少页
        if (1&pg_dir[i]) {
            pg_tbl=(long *) (0xfffff000 & pg_dir[i]);
            for(j=k=0 ; j<1024 ; j++)
                if (pg_tbl[j]&1)
                    k++;
            printk("Pg-dir[%d] uses %d pages\n",i,k);
        }
    }
}
