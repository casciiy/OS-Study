#include <string.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

struct m_inode inode_table[NR_INODE]={{0,},};

static void read_inode(struct m_inode * inode);
static void write_inode(struct m_inode * inode);

static inline void wait_on_inode(struct m_inode * inode)
{
    cli();
    while (inode->i_lock)
        sleep_on(&inode->i_wait);	//��sched.c�ж���
    sti();
}

static inline void lock_inode(struct m_inode * inode)
{
    cli();
    while (inode->i_lock)
        sleep_on(&inode->i_wait);
    inode->i_lock=1;
    sti();
}

static inline void unlock_inode(struct m_inode * inode)
{
    inode->i_lock=0;
    wake_up(&inode->i_wait);				//��sched.c�ж���
}

void sync_inodes(void)
{
    int i;
    struct m_inode * inode;

    inode = 0+inode_table;
    for(i=0 ; i<NR_INODE ; i++,inode++) {
        wait_on_inode(inode);
        if (inode->i_dirt && !inode->i_pipe)
            write_inode(inode);
    }
}

static int _bmap(struct m_inode * inode,int block,int create)		//�ļ���Ӳ��ӳ��
{
    struct buffer_head * bh;
    int i;

    if (block<0)
        panic("_bmap: block<0");
    if (block >= 7+512+512*512)			//�˿��Ƿ񳬹��������ļ�Ӱ������ʾ�Ŀ�
        panic("_bmap: block>big");
    if (block<7) {
        if (create && !inode->i_zone[block])	//�����Ҫ���������Ҹ�ֱ�ӿ���null��
            if (inode->i_zone[block]=new_block(inode->i_dev)) {	//�����¿飬���ҵ��Ŀ����i_zone��
                inode->i_ctime=CURRENT_TIME;		//�޸�ʱ��
                inode->i_dirt=1;				//Ҫ��д��
            }
        return inode->i_zone[block];
    }
    block -= 7;
    if (block<512) {				//����˿�λ��һ����ӿ�
        if (create && !inode->i_zone[7])
            if (inode->i_zone[7]=new_block(inode->i_dev)) {
                inode->i_dirt=1;
                inode->i_ctime=CURRENT_TIME;
            }
        if (!inode->i_zone[7])
            return 0;
        if (!(bh = bread(inode->i_dev,inode->i_zone[7])))	//��һ����ӿ���뻺����
            return 0;
        i = ((unsigned short *) (bh->b_data))[block];	//��һ����ӿ����������ҵ���Ӧ���
        if (create && !i)
            if (i=new_block(inode->i_dev)) {
                ((unsigned short *) (bh->b_data))[block]=i;	//���ҵ���ʵ�ʿ�д��һ����ӿ�������
                bh->b_dirt=1;
            }
        brelse(bh);
        return i;
    }
    block -= 512;			//����Ƕ�����ӿ�
    if (create && !inode->i_zone[8])
        if (inode->i_zone[8]=new_block(inode->i_dev)) {
            inode->i_dirt=1;
            inode->i_ctime=CURRENT_TIME;
        }
    if (!inode->i_zone[8])
        return 0;
    if (!(bh=bread(inode->i_dev,inode->i_zone[8])))
        return 0;
    i = ((unsigned short *)bh->b_data)[block>>9];
    if (create && !i)
        if (i=new_block(inode->i_dev)) {
            ((unsigned short *) (bh->b_data))[block>>9]=i;
            bh->b_dirt=1;
        }
    brelse(bh);
    if (!i)
        return 0;
    if (!(bh=bread(inode->i_dev,i)))
        return 0;
    i = ((unsigned short *)bh->b_data)[block&511];
    if (create && !i)
        if (i=new_block(inode->i_dev)) {
            ((unsigned short *) (bh->b_data))[block&511]=i;
            bh->b_dirt=1;
        }
    brelse(bh);
    return i;
}

int bmap(struct m_inode * inode,int block)
{
    return _bmap(inode,block,0);
}

int create_block(struct m_inode * inode, int block)
{
    return _bmap(inode,block,1);
}

void iput(struct m_inode * inode)		//�ͷŸýڵ㼰�ýڵ�ʹ�õ����п飨���ڴ�map�У�
{
    if (!inode)
        return;
    wait_on_inode(inode);
    if (!inode->i_count)
        panic("iput: trying to free free inode");
    if (inode->i_pipe) {			//����ýڵ���һ���ܵ�
        wake_up(&inode->i_wait);
        if (--inode->i_count)	//����˽ڵ��ǹ���ģ�ֱ�ӷ���
            return;
        free_page(inode->i_size);	//���û�н����ô˽ڵ㣬�ͷŸ�ҳ
        inode->i_count=0;
        inode->i_dirt=0;
        inode->i_pipe=0;
        return;
    }
    if (!inode->i_dev || inode->i_count>1) {
        inode->i_count--;
        return;
    }
repeat:
    if (!inode->i_nlinks) {
        truncate(inode);		//�ͷŴ˽ڵ������ֱ�Ӻͼ�ӿ��map
        free_inode(inode);		//��s_imap��������ýڵ�
        return;
    }
    if (inode->i_dirt) {
        write_inode(inode);	/* we can sleep - so do again */
        wait_on_inode(inode);
        goto repeat;
    }
    inode->i_count--;
    return;
}

static volatile int last_allocated_inode = 0;

struct m_inode * get_empty_inode(void)
{
    struct m_inode * inode;
    int inr;

    while (1) {
        inode = NULL;
        inr = last_allocated_inode;
        do {						//���ѭ������inode_table��ȡ��һ���ڴ�ڵ�
            if (!inode_table[inr].i_count) {
                inode = inr + inode_table;
                break;
            }
            inr++;
            if (inr>=NR_INODE)
                inr=0;
        } while (inr != last_allocated_inode);
        if (!inode) {					//����ڵ�Ϊ�գ���ô����
            for (inr=0 ; inr<NR_INODE ; inr++)
                printk("%04x: %6d\t",inode_table[inr].i_dev,
                       inode_table[inr].i_num);
            panic("No free inodes in mem");
        }
        last_allocated_inode = inr;
        wait_on_inode(inode);
        while (inode->i_dirt) {			//����ڵ�����ģ���ôд��
            write_inode(inode);
            wait_on_inode(inode);
        }
        if (!inode->i_count)
            break;
    }
    memset(inode,0,sizeof(*inode));			//���˽ڵ��ʼ��Ϊ0
    inode->i_count = 1;					//����ѷ���
    return inode;
}

struct m_inode * get_pipe_inode(void)
{
    struct m_inode * inode;

    if (!(inode = get_empty_inode()))			//Ϊ�ܵ�ȡһ���յĽڵ�
        return NULL;
    if (!(inode->i_size=get_free_page())) {		//Ϊ����ڵ�����ڴ�
        inode->i_count = 0;
        return NULL;
    }
    inode->i_count = 2;	/* sum of readers/writers */
    PIPE_HEAD(*inode) = PIPE_TAIL(*inode) = 0;	//��ʼ���ܵ�Ϊ0
    inode->i_pipe = 1;					//��Ǹýڵ���һ���ܵ�
    return inode;
}

struct m_inode * iget(int dev,int nr)
{
    struct m_inode * inode, * empty;

    if (!dev)
        panic("iget with dev==0");
    empty = get_empty_inode();				//ȡһ�����е��ڴ�ڵ�
    inode = inode_table;					//inodeָ��inode_table[0]
    while (inode < NR_INODE+inode_table) {
        if (inode->i_dev != dev || inode->i_num != nr) {	//Ѱ���Ƿ���dev��nr���ڴ�ڵ�
            inode++;
            continue;
        }
        wait_on_inode(inode);			//�ȴ��ͷŴ˽ڵ�
        if (inode->i_dev != dev || inode->i_num != nr) {
            inode = inode_table;
            continue;
        }
        inode->i_count++;
        if (empty)
            iput(empty);
        return inode;
    }
    //���´���������ڵ�ǰ��inode_table��û���ֳɵģ���Ҫִ�е�
    if (!empty)
        return (NULL);
    inode=empty;
    inode->i_dev = dev;
    inode->i_num = nr;
    read_inode(inode);
    return inode;
}

static void read_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;
    int block;

    lock_inode(inode);
    sb=get_super(inode->i_dev);				//ȡ����Ӧ��i_dev���ڵĳ�����
    block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
            (inode->i_num-1)/INODES_PER_BLOCK;		//��������
    if (!(bh=bread(inode->i_dev,block)))
        panic("unable to read i-node block");	//��ȡ�ڵ����ڵĿ����buffer
    *(struct d_inode *)inode =
        ((struct d_inode *)bh->b_data)
        [(inode->i_num-1)%INODES_PER_BLOCK];	//������ýڵ���������λ��
    brelse(bh);
    unlock_inode(inode);
}

static void write_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;
    int block;

    lock_inode(inode);
    sb=get_super(inode->i_dev);				//get_super��fs.h�ж���
    block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
            (inode->i_num-1)/INODES_PER_BLOCK;
    if (!(bh=bread(inode->i_dev,block)))
        panic("unable to read i-node block");
    ((struct d_inode *)bh->b_data)
    [(inode->i_num-1)%INODES_PER_BLOCK] =
        *(struct d_inode *)inode;
    bh->b_dirt=1;
    inode->i_dirt=0;
    brelse(bh);
    unlock_inode(inode);
}
