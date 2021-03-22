#include <signal.h>

#include <linux/config.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <asm/system.h>

int do_exit(long code);

#define invalidate() \
//�������Ƕ������˼�ǣ�
//	movl	0	%eax
//	movl	%eax	%cr3		//��ҳĿ¼���������ַ��Ϊ0
__asm__("movl %%eax,%%cr3"::"a" (0))

#if (BUFFER_END < 0x100000)		//0x200000=2M
#define LOW_MEM 0x100000
#else
#define LOW_MEM BUFFER_END
#endif

/* these are not to be changed - thay are calculated from the above */
#define PAGING_MEMORY (HIGH_MEMORY - LOW_MEM)	//��ҳ�ڴ���6M
#define PAGING_PAGES (PAGING_MEMORY/4096)	//�ж��ٸ�ҳ=1536��ҳ
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)		//ҳ��

#if (PAGING_PAGES < 10)
#error "Won't work"
#endif

#define copy_page(from,to) \
//�������Ƕ������˼�ǣ�
//	movl	1024	%ecx
//	movl	from	%esi
//	movl	to	%edi
//	cld
//	rep	movsl
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024):"cx","di","si")

static unsigned short mem_map [ PAGING_PAGES ] = {0,};	//���ĳҳ����1��˵����ҳ������

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
unsigned long get_free_page(void)
{
    register unsigned long __res asm("ax");
    //�������Ƕ������˼�ǣ�
    //	movl	0	%eax
    //	movl	LOW_MEM	%ebx
    //	movl	PAGING_PAGES	%ecx
    //	movl	mem_map+PAGING_PAGES-1	%edi
    //	std							//��ַ��������
    //	repne						//ZF=0ʱѭ��
    //	scasw						//eax---->es:di
    //	jne	1f						//ÿҳ�����ã�ֱ�ӷ���
    //	movw	$1	%edi+2			//��mem_map[X]=1�����е�X��Ѱ�ҵ��ĵڼ���ҳ
    //	sall	$12	%ecx				//PAGING_PAGES��4096�����������ֽ�Ϊ��λ��Ѱ��ҳ�Ļ�ַ
    //	movl	%ecx	%edx		//�����ַ
    //	addl	%ebx	%edx			//��ַ��BUFFER_END��Ϊʵ�ʵ�ַ
    //	movl	$1024	%ecx
    //	leal	%edx+4092	%edi		//%edx+4092����Ϊǰ����std������Ҫ����4
    //	rep
    //	stosl							//eax--->es:edi����Ҫ�ǽ���ҳ��ʼ��Ϊ0
    //	movl	%edx	%eax		//���˻�ַ������eax
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
    if (addr<LOW_MEM) return;			//���ܵ���2M
    if (addr>HIGH_MEMORY)				//���ܸ���8M
        panic("trying to free nonexistent page");
    //����2�л�ԭ�˻�ַ��mem_map[]�ĵ�ַ
    addr -= LOW_MEM;
    addr >>= 12;
    if (mem_map[addr]--) return;			//����ǹ����ڴ棬��FREE
    mem_map[addr]=0;					//FREE��ҳ
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

    if (from & 0x3fffff)			//�����ܱ�4M��������С��ӦΪ0x400000
        panic("free_page_tables called with wrong alignment");
    if (!from)					//����Ϊ0������FREE�ں���
        panic("Trying to free up swapper memory space");
    size = (size + 0x3fffff) >> 22;	//1����4M��2����8M����������
    dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */	//�ҳ�Ŀ¼��ַ���õ�ַ����copy_page_tables���ã�����from>>20��ֻ�ƶ�20λ��������22λ������Ϊ�Ժ�Ҫ��4Ѱַ������0xffc��ȡ32λ��ַ�����10λ����4
    for ( ; size-->0 ; dir++) {
        if (!(1 & *dir))
            continue;
        pg_table = (unsigned long *) (0xfffff000 & *dir);	//��Ŀ¼��ַ��ȡ��ҳ���ַ
        for (nr=0 ; nr<1024 ; nr++) {
            if (1 & *pg_table)		//�����ҳ�����ڴ��У���FREE��
                free_page(0xfffff000 & *pg_table);
            *pg_table = 0;			//����ҳ�����ָ��ָ��������Ϊ0
            pg_table++;			//ָ����һ��ҳ����
        }
        free_page(0xfffff000 & *dir);	//FREE��Ŀ¼��ַ������ָ���ڴ��ַ
        *dir = 0;					//��Ǵ�Ŀ¼����
    }
    invalidate();					//��cr3��Ϊ0
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

    if ((from&0x3fffff) || (to&0x3fffff))		//from��to���뱻4M������4M�߽����
        panic("copy_page_tables called with wrong alignment");
    from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */	��//ȡfrom��10λ��Ȼ���4���γ�Ŀ¼��
    to_dir = (unsigned long *) ((to>>20) & 0xffc);
    size = ((unsigned) (size+0x3fffff)) >> 22;
    for( ; size-->0 ; from_dir++,to_dir++) {
        if (1 & *to_dir)		//����to_dirָ��ָ�����ڴ��ַ��ֵ�����λ�Ƿ�Ϊ1����Ϊ1��˵����ֵ����ʾ�ĵ�ַ�ѷ���
            panic("copy_page_tables: already exist");
        if (!(1 & *from_dir))	//from_dirָ���е�ֵ��ʾ�ĵ�ַ�����ѱ�����
            continue;
        from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
        if (!(to_page_table = (unsigned long *) get_free_page()))	//����һ�����������ڴ��ַ����
            return -1;	/* Out of memory, see freeing */
        *to_dir = ((unsigned long) to_page_table) | 7;		//��to_dir��ָ���ڴ�����д��ȡ�õ������ڴ��ַ�������ֵ�����λΪ1����ʾ�ѷ���
        nr = (from==0)?0xA0:1024;		//160X4K=640K
        for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
            this_page = *from_page_table;	����//��Դҳ��ָ����ָ��ֵ����this_page
            if (!(1 & this_page))
                continue;
            this_page &= ~2;		����������	//����ֵ��ʾ�ĵ�ַ����͵�2λ��Ϊ0��ֻ������3�Ľ��̶�
            *to_page_table = this_page;			//��Դҳ���е�ҳ����ֵ����to_page_tableָ����ָ���ڴ��ַ��
            if (this_page > LOW_MEM) {		//�Ƿ����ں�����
                *from_page_table = this_page;	//��Դҳ����ĵ�2λҲ��Ϊ0��ԭ���ǹ�����
                this_page -= LOW_MEM;		//����ҳӳ����1����ʾ����
                this_page >>= 12;
                mem_map[this_page]++;
            }
        }
    }
    invalidate();								//��cr3����Ļ�ַ��Ϊ0
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

    if (page < LOW_MEM || page > HIGH_MEMORY)��	//������2M��������8M
        printk("Trying to put page %p at %p\n",page,address);
    if (mem_map[(page-LOW_MEM)>>12] != 1)		����//��ҳ�Ƿ����ڴ���
        printk("mem_map disagrees with %p at %p\n",page,address);
    page_table = (unsigned long *) ((address>>20) & 0xffc);����//��ָ��ӦΪҳĿ¼ָ��
    if ((*page_table)&1)					��������������//��ҳĿ¼�Ƿ����ڴ���
        page_table = (unsigned long *) (0xfffff000 & *page_table);
    else {
        if (!(tmp=get_free_page()))		����������������	//���ҳĿ¼�����ڴ��У���һ������ҳ���Ѹ�ҳ�ĵ�ַ��Ϊҳ����ָ��
            return 0;
        *page_table = tmp|7;
        page_table = (unsigned long *) tmp;
    }
    page_table[(address>>12) & 0x3ff] = page | 7;	//�õ�ַ�����page_table�׵�ַ�����λ�ã��ڴ˴�����page|7
    return page;
}

void un_wp_page(unsigned long * table_entry)	//�˺����������Ǳ����ں���ҳ�����ڴ�ҳ
{
    unsigned long old_page,new_page;

    old_page = 0xfffff000 & *table_entry;
    if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {	//�����ҳ�����ں�������ҳ�ѷ�������
        *table_entry |= 2;				����//ֻ����ҳ�ĵ�2λ��1��ʹ��ҳ�ɶ���д
        return;					��������//ֱ�ӷ���
    }
    if (!(new_page=get_free_page()))
        do_exit(SIGSEGV);
    if (old_page >= LOW_MEM)
        mem_map[MAP_NR(old_page)]--;	//������ҳ�ļ�����1
    *table_entry = new_page | 7;			����//������¶���Ϊ��ҳ�ĵ�ַ
    copy_page(old_page,new_page);			//����ҳ��������ҳ
}

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 */
void do_wp_page(unsigned long error_code,unsigned long address)
{
    un_wp_page((unsigned long *)
               (((address>>10) & 0xffc) + (0xfffff000 &*((unsigned long *) ((address>>20) &0xffc)))));		��������������������������������//����((address>>10) & 0xffc)������׵�ַ��ƫ�Ƶ�ַ����ȡaddress��12��21λ��10λ��(0xfffff000 &*((unsigned long *) ((address>>20) &0xffc))))�Ǽ����ҳ���׵�ַ

}

void write_verify(unsigned long address)
{
    unsigned long page;

    if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
        return;					����//���ҳĿ¼ָ���ҳ�����ڴ棬ֱ�ӷ���
    page &= 0xfffff000;					//ȡҳ���׵�ַ
    page += ((address>>10) & 0xffc);		//����address�е��м�10λ������4�����γ���ҳ�����ַ
    if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */	//�Թ����ڴ�Ĵ���
        un_wp_page((unsigned long *) page);
    return;
}

void do_no_page(unsigned long error_code,unsigned long address)	//���û��ҳ��ȡһ������ҳ��������ָ����ҳ���ַ��address����
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
        if (!mem_map[i]) free++;		������������������	//�����ж�������ҳ
    printk("%d pages free (of %d)\n\r",free,PAGING_PAGES);
    for(i=2 ; i<1024 ; i++) {			��������	//�����ҳĿ¼�е�ҳ���õ��˶���ҳ
        if (1&pg_dir[i]) {
            pg_tbl=(long *) (0xfffff000 & pg_dir[i]);
            for(j=k=0 ; j<1024 ; j++)
                if (pg_tbl[j]&1)
                    k++;
            printk("Pg-dir[%d] uses %d pages\n",i,k);
        }
    }
}
