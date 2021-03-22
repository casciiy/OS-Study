/* bitmap.c contains the code that handles the inode and block bitmaps */
#include <string.h>

#include <linux/sched.h>
#include <linux/kernel.h>

#define clear_block(addr) \
//此内嵌汇编的意思为：
//	movl	0	%eax
//	movl	BLOCK_size/4	%edi
//	movl	*addr	%edi
//	cld
//	rep stosl	//将eax传送到es:edi
__asm__("cld\n\t" \
        "rep\n\t" \
        "stosl" \
        ::"a" (0),"c" (BLOCK_SIZE/4),"D" ((long) (addr)):"cx","di")

#define set_bit(nr,addr) ({\
register int res __asm__("ax"); \
//此内嵌汇编的意思为：
        //	movl	0	%eax
        //	movl	nr	%ebx
        //	movl	*addr	%edx
        //	btsl	%ebx	%edx	//测试位，将该位送CF，并将该位置1
        //	setb	%al		//CF=1，则al=1，反之为0
        //	movl	%eax	res
        __asm__("btsl %2,%3\n\tsetb %%al":"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})

#define clear_bit(nr,addr) ({\
register int res __asm__("ax"); \
//此内嵌汇编的意思为：
//	movl	0	%eax
//	movl	nr	%ebx
//	movl	*addr	%ecx
//	btrl	%ebx	%ecx
//	setnb	%al		//CF=0,al=1,反之为0
//	movl	%eax	res
__asm__("btrl %2,%3\n\tsetnb %%al":"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
        res;})

#define find_first_zero(addr) ({ \
int __res; \
//此内嵌汇编的意思为：
//	movl	0	%ecx
//	movl	*addr	%esi
//	cld
//	1:	lodsl		//将esi送入eax
//		notl	%eax	//求反
//		bsfl	%eax	%edx	//测试eax中的各位，当遇到有1的位，ZF=0，且将该位的序号存入edx，如果eax中所有位为0，则ZF=1，且edx中的值无意义
//		je	2f
//		addl	%edx	%ecx
//		jmp	3f
//	2:	addl	$32	%ecx
//		cmpl	$8192	%ecx		//$8192==8K
//		jl	1b
//	3:	movl	%eax	_res
__asm__("cld\n" \
        "1:\tlodsl\n\t" \
        "notl %%eax\n\t" \
        "bsfl %%eax,%%edx\n\t" \
        "je 2f\n\t" \
        "addl %%edx,%%ecx\n\t" \
        "jmp 3f\n" \
        "2:\taddl $32,%%ecx\n\t" \
        "cmpl $8192,%%ecx\n\t" \
        "jl 1b\n" \
        "3:" \
        :"=c" (__res):"c" (0),"S" (addr):"ax","dx","si"); \
        __res;})

void free_block(int dev, int block)
{
    struct super_block * sb;
    struct buffer_head * bh;

    if (!(sb = get_super(dev)))		//在超级块表中找包括dev的超级块
        panic("trying to free block on nonexistent device");
    if (block < sb->s_firstdatazone || block >= sb->s_nzones)	//这个块是否小于超级所表示的第一数据区域或该块是否超过了超级块所表示的最大数据区域
        panic("trying to free block not in datazone");
    bh = get_hash_table(dev,block);
    if (bh) {
        if (bh->b_count != 1) {	//如果这个bh是共享的，就不释放
            printk("trying to free block (%04x:%d), count=%d\n",
                   dev,block,bh->b_count);
            return;
        }
        bh->b_dirt=0;
        bh->b_uptodate=0;
        brelse(bh);		//释放此bh
    }
    block -= sb->s_firstdatazone - 1 ;		//此时块代表在超级块的相对区域，即在超级块中的第几块区域
    if (clear_bit(block&8191,sb->s_zmap[block/8192]->b_data)) {	//测试该相对块位置在超级块中的区域映射中表示的数据位是否为1，并从映射中清除该位，即表示该块不在内存的区域MAP中
        printk("block (%04x:%d) ",dev,block+sb->s_firstdatazone-1);
        panic("free_block: bit already cleared");
    }
    sb->s_zmap[block/8192]->b_dirt = 1;	//表示已修改了超级块区域映射表，需要写盘
}

int new_block(int dev)
{
    struct buffer_head * bh;
    struct super_block * sb;
    int i,j;

    if (!(sb = get_super(dev)))		//取得该设备文件的超级块
        panic("trying to get new block from nonexistant device");
    j = 8192;				//8192==8K
    for (i=0 ; i<8 ; i++)
        if (bh=sb->s_zmap[i])
            if ((j=find_first_zero(bh->b_data))<8192)		//在s_zmap中寻找一个空的表项
                break;
    if (i>=8 || !bh || j>=8192)		//未找到合法的表项，返回0
        return 0;
    if (set_bit(j,bh->b_data))		//设置这个刚寻找到的空位，标识其已分配
        panic("new_block: bit already set");
    bh->b_dirt = 1;			//需要写盘
    j += i*8192 + sb->s_firstdatazone-1;	//计算出此块在磁盘中的实际位置
    if (j >= sb->s_nzones)		//不能超过超级块的范围
        return 0;
    if (!(bh=getblk(dev,j)))		//分配一个buffer为这个dev文件新块
        panic("new_block: cannot get block");
    if (bh->b_count != 1)
        panic("new block: count is != 1");
    clear_block(bh->b_data);		//将此buffer数据区清0
    bh->b_uptodate = 1;
    bh->b_dirt = 1;
    brelse(bh);				//释放该BUFFER
    return j;
}

void free_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;

    if (!inode)
        return;
    if (!inode->i_dev) {			//如果这是一个EMPTY INODE
        memset(inode,0,sizeof(*inode));	//将此节点的各位初始化为0
        return;
    }
    if (inode->i_count>1) {		//如果是共享节点，出错
        printk("trying to free inode with count=%d\n",inode->i_count);
        panic("free_inode");
    }
    if (inode->i_nlinks)
        panic("trying to free inode with links");
    if (!(sb = get_super(inode->i_dev)))
        panic("trying to free inode on nonexistent device");
    if (inode->i_num < 1 || inode->i_num > sb->s_ninodes)
        panic("trying to free inode 0 or nonexistant inode");
    if (!(bh=sb->s_imap[inode->i_num>>13]))		//除以8K
        panic("nonexistent imap in superblock");
    if (clear_bit(inode->i_num&8191,bh->b_data))	//在s_imap表中清除该号的节点影像
        panic("free_inode: bit already cleared");
    bh->b_dirt = 1;			//要写盘
    memset(inode,0,sizeof(*inode));
}

struct m_inode * new_inode(int dev)
{
    struct m_inode * inode;
    struct super_block * sb;
    struct buffer_head * bh;
    int i,j;

    if (!(inode=get_empty_inode()))	//在内存节点缓冲组中取一个空闲的节点
        return NULL;
    if (!(sb = get_super(dev)))		//取DEV文件的超级块
        panic("new_inode with unknown device");
    j = 8192;				//==8K
    for (i=0 ; i<8 ; i++)
        if (bh=sb->s_imap[i])	//在s_imap中发现一个空位置
            if ((j=find_first_zero(bh->b_data))<8192)
                break;
    if (!bh || j >= 8192 || j+i*8192 > sb->s_ninodes) {
        iput(inode);
        return NULL;
    }
    if (set_bit(j,bh->b_data))		//将在s_imap中发现的空位置置1，也就是标记在用
        panic("new_inode: bit already set");
    bh->b_dirt = 1;			//要写盘
    inode->i_count=1;
    inode->i_nlinks=1;
    inode->i_dev=dev;
    inode->i_dirt=1;
    inode->i_num = j + i*8192;		//计算出此节点的在磁盘中相应块号
    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
    return inode;
}
