/*
 * super.c contains code to handle the super-block tables.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>

/* set_bit uses setb, as gas doesn't recognize setc */
#define set_bit(bitnr,addr) ({ \			//���Ը�λ�����ø�λ
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

struct super_block super_block[NR_SUPER];

struct super_block * do_mount(int dev)
{
    struct super_block * p;
    struct buffer_head * bh;
    int i,block;

    for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; p++ )		//Ѱ��һ�����еĳ����飬�����������
        if (!(p->s_dev))
            break;
    p->s_dev = -1;		/* mark it in use */
    if (p >= &super_block[NR_SUPER])
        return NULL;
    if (!(bh = bread(dev,1)))					//��ȡ�����ĵ�2�飬��������
        return NULL;
    *p = *((struct super_block *) bh->b_data);		//�����������������ʽ��Ϊsuper�ṹ
    brelse(bh);
    if (p->s_magic != SUPER_MAGIC) {				//����SUPER_MAGIC=0x137F���Ǳ�ʶ�ļ�����
        p->s_dev = 0;
        return NULL;
    }
    for (i=0;i<I_MAP_SLOTS;i++)					//8�����ͣ���ʼ��8���豸�ļ���inodeӳ��ں�zoneӳ���
        p->s_imap[i] = NULL;
    for (i=0;i<Z_MAP_SLOTS;i++)
        p->s_zmap[i] = NULL;
    block=2;
    for (i=0 ; i < p->s_imap_blocks ; i++)			//����Щ�豸�ļ���inodeӳ��ڶ��뻺����
        if (p->s_imap[i]=bread(dev,block))
            block++;
        else
            break;
    for (i=0 ; i < p->s_zmap_blocks ; i++)			//����Щ�豸�ļ���zoneӳ��ڶ��뻺����
        if (p->s_zmap[i]=bread(dev,block))
            block++;
        else
            break;
    if (block != 2+p->s_imap_blocks+p->s_zmap_blocks) {	//������ɹ����ͷ���Щ������
        for(i=0;i<I_MAP_SLOTS;i++)
            brelse(p->s_imap[i]);
        for(i=0;i<Z_MAP_SLOTS;i++)
            brelse(p->s_zmap[i]);
        p->s_dev=0;
        return NULL;
    }
    p->s_imap[0]->b_data[0] |= 1;				//�����ǳ�ʼ������������
    p->s_zmap[0]->b_data[0] |= 1;
    p->s_dev = dev;
    p->s_isup = NULL;
    p->s_imount = NULL;
    p->s_time = 0;
    p->s_rd_only = 0;
    p->s_dirt = 0;
    return p;
}

void mount_root(void)						//���Ĺ������ǵǼǸ�������
{
    int i,free;
    struct super_block * p;
    struct m_inode * mi;

    if (32 != sizeof (struct d_inode))
        panic("bad i-node size");
    for(i=0;i<NR_FILE;i++)
        file_table[i].f_count=0;
    for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; p++)	//��ʼ���������б�ʹ���ǵ�s_devΪ0
        p->s_dev = 0;
    if (!(p=do_mount(ROOT_DEV)))				//Ӳ�̶�д����ȡ�����飬����������������䳬����ṹ
        panic("Unable to mount root");
    if (!(mi=iget(ROOT_DEV,1)))					//ȡ�ø��ڵ�
        panic("Unable to read root i-node");
    mi->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */
    p->s_isup = p->s_imount = mi;				//�����ڵ���볬����ṹ�У����ڸýṹ�а�װ���ڵ�
    current->pwd = mi;						//������ָ����ڵ�
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
