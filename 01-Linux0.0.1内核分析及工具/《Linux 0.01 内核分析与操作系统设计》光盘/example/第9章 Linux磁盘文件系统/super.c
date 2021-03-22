/*
 * super.c contains code to handle the super-block tables.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>

/* set_bit uses setb, as gas doesn't recognize setc */
#define set_bit(bitnr,addr) ({ \			//测试该位并设置该位
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

struct super_block super_block[NR_SUPER];

struct super_block * do_mount(int dev)
{
    struct super_block * p;
    struct buffer_head * bh;
    int i,block;

    for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; p++ )		//寻找一个空闲的超级块，并标记它在用
        if (!(p->s_dev))
            break;
    p->s_dev = -1;		/* mark it in use */
    if (p >= &super_block[NR_SUPER])
        return NULL;
    if (!(bh = bread(dev,1)))					//读取分区的第2块，即超级块
        return NULL;
    *p = *((struct super_block *) bh->b_data);		//将超级块的数据区格式化为super结构
    brelse(bh);
    if (p->s_magic != SUPER_MAGIC) {				//其中SUPER_MAGIC=0x137F，是标识文件类型
        p->s_dev = 0;
        return NULL;
    }
    for (i=0;i<I_MAP_SLOTS;i++)					//8种类型，初始化8个设备文件的inode映射口和zone映射口
        p->s_imap[i] = NULL;
    for (i=0;i<Z_MAP_SLOTS;i++)
        p->s_zmap[i] = NULL;
    block=2;
    for (i=0 ; i < p->s_imap_blocks ; i++)			//将这些设备文件的inode映射口读入缓冲区
        if (p->s_imap[i]=bread(dev,block))
            block++;
        else
            break;
    for (i=0 ; i < p->s_zmap_blocks ; i++)			//将这些设备文件的zone映射口读入缓冲区
        if (p->s_zmap[i]=bread(dev,block))
            block++;
        else
            break;
    if (block != 2+p->s_imap_blocks+p->s_zmap_blocks) {	//如果不成功，释放这些缓冲区
        for(i=0;i<I_MAP_SLOTS;i++)
            brelse(p->s_imap[i]);
        for(i=0;i<Z_MAP_SLOTS;i++)
            brelse(p->s_zmap[i]);
        p->s_dev=0;
        return NULL;
    }
    p->s_imap[0]->b_data[0] |= 1;				//以下是初始化超级块数据
    p->s_zmap[0]->b_data[0] |= 1;
    p->s_dev = dev;
    p->s_isup = NULL;
    p->s_imount = NULL;
    p->s_time = 0;
    p->s_rd_only = 0;
    p->s_dirt = 0;
    return p;
}

void mount_root(void)						//它的工作就是登记根超级块
{
    int i,free;
    struct super_block * p;
    struct m_inode * mi;

    if (32 != sizeof (struct d_inode))
        panic("bad i-node size");
    for(i=0;i<NR_FILE;i++)
        file_table[i].f_count=0;
    for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; p++)	//初始化超级块列表，使他们的s_dev为0
        p->s_dev = 0;
    if (!(p=do_mount(ROOT_DEV)))				//硬盘读写，读取超级块，并用其数据区来填充超级块结构
        panic("Unable to mount root");
    if (!(mi=iget(ROOT_DEV,1)))					//取得根节点
        panic("Unable to read root i-node");
    mi->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */
    p->s_isup = p->s_imount = mi;				//将根节点放入超级块结构中，即在该结构中安装根节点
    current->pwd = mi;						//将他们指向根节点
    current->root = mi;
    free=0;
    i=p->s_nzones;
    while (-- i >= 0)
        if (!set_bit(i&8191,p->s_zmap[i>>13]->b_data))	//8192=8K
            free++;
    printk("%d/%d free blocks\n\r",free,p->s_nzones);
    free=0;
    i=p->s_ninodes+1;
    while (-- i >= 0)
        if (!set_bit(i&8191,p->s_imap[i>>13]->b_data))
            free++;
    printk("%d/%d free inodes\n\r",free,p->s_ninodes);
}
