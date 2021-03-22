/*
 *  'buffer.c' implements the buffer-cache functions. Race-conditions have
 * been avoided by NEVER letting a interrupt change a buffer (except for the
 * data, of course), but instead letting the caller do it. NOTE! As interrupts
 * can wake up a caller, some cli-sti sequences are needed to check for
 * sleep-on-calls. These should be extremely quick, though (I hope).
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#if (BUFFER_END & 0xfff)
#error "Bad BUFFER_END value"
#endif

#if (BUFFER_END > 0xA0000 && BUFFER_END <= 0x100000)
#error "Bad BUFFER_END value"
#endif

extern int end;			//占位符
struct buffer_head * start_buffer = (struct buffer_head *) &end;
struct buffer_head * hash_table[NR_HASH];
static struct buffer_head * free_list;
static struct task_struct * buffer_wait = NULL;
int NR_BUFFERS = 0;

static inline void wait_on_buffer(struct buffer_head * bh)
{
    cli();
    while (bh->b_lock)
        sleep_on(&bh->b_wait);	//在sched.c中定义
    sti();
}

int sys_sync(void)
{
    int i;
    struct buffer_head * bh;

    sync_inodes();		/* write out inodes into buffers */	//向磁盘写节点，在inode.c中定义
    bh = start_buffer;
    for (i=0 ; i<NR_BUFFERS ; i++,bh++) {
        wait_on_buffer(bh);
        if (bh->b_dirt)
            ll_rw_block(WRITE,bh);
    }
    return 0;
}

static int sync_dev(int dev)
{
    int i;
    struct buffer_head * bh;

    bh = start_buffer;
    for (i=0 ; i<NR_BUFFERS ; i++,bh++) {
        if (bh->b_dev != dev)
            continue;
        wait_on_buffer(bh);
        if (bh->b_dirt)
            ll_rw_block(WRITE,bh);	//将缓冲区的数据写盘，在block_dev.c中定义
    }
    return 0;
}

#define _hashfn(dev,block) (((unsigned)(dev^block))%NR_HASH)
#define hash(dev,block) hash_table[_hashfn(dev,block)]

static inline void remove_from_queues(struct buffer_head * bh)	//将找到的缓冲区从空闲缓冲区列表中摘下
{
    /* remove from hash-queue */
    if (bh->b_next)
        bh->b_next->b_prev = bh->b_prev;
    if (bh->b_prev)
        bh->b_prev->b_next = bh->b_next;
    if (hash(bh->b_dev,bh->b_blocknr) == bh)
        hash(bh->b_dev,bh->b_blocknr) = bh->b_next;
    /* remove from free list */
    if (!(bh->b_prev_free) || !(bh->b_next_free))
        panic("Free block list corrupted");
    bh->b_prev_free->b_next_free = bh->b_next_free;
    bh->b_next_free->b_prev_free = bh->b_prev_free;
    if (free_list == bh)
        free_list = bh->b_next_free;
}

static inline void insert_into_queues(struct buffer_head * bh)	//将已变成空闲状态的缓冲区插入到空闲缓冲区列表中
{
    /* put at end of free list */
    bh->b_next_free = free_list;
    bh->b_prev_free = free_list->b_prev_free;
    free_list->b_prev_free->b_next_free = bh;
    free_list->b_prev_free = bh;
    /* put the buffer in new hash-queue if it has a device */
    bh->b_prev = NULL;
    bh->b_next = NULL;
    if (!bh->b_dev)
        return;
    bh->b_next = hash(bh->b_dev,bh->b_blocknr);
    hash(bh->b_dev,bh->b_blocknr) = bh;
    bh->b_next->b_prev = bh;
}

static struct buffer_head * find_buffer(int dev, int block)		//发现所需要的缓冲区
{
    struct buffer_head * tmp;

    for (tmp = hash(dev,block) ; tmp != NULL ; tmp = tmp->b_next)
        if (tmp->b_dev==dev && tmp->b_blocknr==block)
            return tmp;
    return NULL;
}

/*
 * Why like this, I hear you say... The reason is race-conditions.
 * As we don't lock buffers (unless we are readint them, that is),
 * something might happen to it while we sleep (ie a read-error
 * will force it bad). This shouldn't really happen currently, but
 * the code is ready.
 */
struct buffer_head * get_hash_table(int dev, int block)
{
    struct buffer_head * bh;

repeat:							//重复，直到所需要的缓冲区被返回
    if (!(bh=find_buffer(dev,block)))
        return NULL;
    bh->b_count++;
    wait_on_buffer(bh);
    if (bh->b_dev != dev || bh->b_blocknr != block) {
        brelse(bh);
        goto repeat;
    }
    return bh;
}

/*
 * Ok, this is getblk, and it isn't very clear, again to hinder
 * race-conditions. Most of the code is seldom used, (ie repeating),
 * so it should be much more efficient than it looks.
 */
struct buffer_head * getblk(int dev,int block)
{
    struct buffer_head * tmp;

repeat:
    if (tmp=get_hash_table(dev,block))		//如果找到了所需要的buffer，那么就直接返回
        return tmp;
    tmp = free_list;		//如果没有找到，即tmp=null，那么就暂时令tmp=free_list，经过缓冲区初始化后，free_list=start_buffer,而sys_setup函数又将start_buffer设置为硬盘分区情况，所以free_list未空
    //以下循环直到找到未用的free_list
    do {
        if (!tmp->b_count) {
            wait_on_buffer(tmp);	/* we still have to wait */
            if (!tmp->b_count)	/* on it, it might be dirty */
                break;
        }
        tmp = tmp->b_next_free;
    } while (tmp != free_list || (tmp=NULL));
    /* Kids, don't try THIS at home ^^^^^. Magic */
    if (!tmp) {				//如果找到的未用的free_list仍为null，那么就重新寻找
        printk("Sleeping on free buffer ..");
        sleep_on(&buffer_wait);
        printk("ok\n");
        goto repeat;
    }
    tmp->b_count++;		//标记找到的可用的free_list为在用
    remove_from_queues(tmp);
    /*
     * Now, when we know nobody can get to this node (as it's removed from the
     * free list), we write it out. We can sleep here without fear of race-
     * conditions.
     */
    if (tmp->b_dirt)
        sync_dev(tmp->b_dev);
    /* update buffer contents */
    tmp->b_dev=dev;
    tmp->b_blocknr=block;
    tmp->b_dirt=0;
    tmp->b_uptodate=0;
    /* NOTE!! While we possibly slept in sync_dev(), somebody else might have
     * added "this" block already, so check for that. Thank God for goto's.
     */
    if (find_buffer(dev,block)) {	//如果在hash队列中又发现了前面已删除的，那么就将当前的tmp插入队列的最后，最后一项永远是free_list
        tmp->b_dev=0;		/* ok, someone else has beaten us */
        tmp->b_blocknr=0;	/* to it - free this block and */
        tmp->b_count=0;		/* try again */
        insert_into_queues(tmp);
        goto repeat;
    }
    /* and then insert into correct position */
    insert_into_queues(tmp);		//重新将它插入队列中
    return tmp;
}

void brelse(struct buffer_head * buf)	//如果不是当前的缓冲区，即为空，那么就等待这个缓冲区变空
{
    if (!buf)
        return;
    wait_on_buffer(buf);
    if (!(buf->b_count--))
        panic("Trying to free free buffer");
    wake_up(&buffer_wait);		//唤醒等待的进程
}

/*
 * bread() reads a specified block and returns the buffer that contains
 * it. It returns NULL if the block was unreadable.
 */
struct buffer_head * bread(int dev,int block)	//读指定设备文件的指定块到缓冲区
{
    struct buffer_head * bh;

    if (!(bh=getblk(dev,block)))	//为这个设备取得一个自由的缓冲区
        panic("bread: getblk returned NULL\n");
    if (bh->b_uptodate)
        return bh;
    ll_rw_block(READ,bh);		//根据bh中的dev和block实际读取硬盘
    if (bh->b_uptodate)
        return bh;
    brelse(bh);				//释放缓冲区
    return (NULL);
}

void buffer_init(void)
{
    struct buffer_head * h = start_buffer;
    void * b = (void *) BUFFER_END;		//==0X200000
    int i;

    while ( (b -= BLOCK_SIZE) >= ((void *) (h+1)) ) {	//初始化缓冲区及它们的数据区，初始化散列表
        h->b_dev = 0;
        h->b_dirt = 0;
        h->b_count = 0;
        h->b_lock = 0;
        h->b_uptodate = 0;
        h->b_wait = NULL;
        h->b_next = NULL;
        h->b_prev = NULL;
        h->b_data = (char *) b;
        h->b_prev_free = h-1;
        h->b_next_free = h+1;
        h++;
        NR_BUFFERS++;
        if (b == (void *) 0x100000)
            b = (void *) 0xA0000;
    }
    h--;
    free_list = start_buffer;
    free_list->b_prev_free = h;
    h->b_next_free = free_list;
    for (i=0;i<NR_HASH;i++)
        hash_table[i]=NULL;
}
