/* bitmap.c contains the code that handles the inode and block bitmaps */
#include <string.h>

#include <linux/sched.h>
#include <linux/kernel.h>

#define clear_block(addr) \
//����Ƕ������˼Ϊ��
//	movl	0	%eax
//	movl	BLOCK_size/4	%edi
//	movl	*addr	%edi
//	cld
//	rep stosl	//��eax���͵�es:edi
__asm__("cld\n\t" \
        "rep\n\t" \
        "stosl" \
        ::"a" (0),"c" (BLOCK_SIZE/4),"D" ((long) (addr)):"cx","di")

#define set_bit(nr,addr) ({\
register int res __asm__("ax"); \
//����Ƕ������˼Ϊ��
        //	movl	0	%eax
        //	movl	nr	%ebx
        //	movl	*addr	%edx
        //	btsl	%ebx	%edx	//����λ������λ��CF��������λ��1
        //	setb	%al		//CF=1����al=1����֮Ϊ0
        //	movl	%eax	res
        __asm__("btsl %2,%3\n\tsetb %%al":"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})

#define clear_bit(nr,addr) ({\
register int res __asm__("ax"); \
//����Ƕ������˼Ϊ��
//	movl	0	%eax
//	movl	nr	%ebx
//	movl	*addr	%ecx
//	btrl	%ebx	%ecx
//	setnb	%al		//CF=0,al=1,��֮Ϊ0
//	movl	%eax	res
__asm__("btrl %2,%3\n\tsetnb %%al":"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
        res;})

#define find_first_zero(addr) ({ \
int __res; \
//����Ƕ������˼Ϊ��
//	movl	0	%ecx
//	movl	*addr	%esi
//	cld
//	1:	lodsl		//��esi����eax
//		notl	%eax	//��
//		bsfl	%eax	%edx	//����eax�еĸ�λ����������1��λ��ZF=0���ҽ���λ����Ŵ���edx�����eax������λΪ0����ZF=1����edx�е�ֵ������
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

    if (!(sb = get_super(dev)))		//�ڳ���������Ұ���dev�ĳ�����
        panic("trying to free block on nonexistent device");
    if (block < sb->s_firstdatazone || block >= sb->s_nzones)	//������Ƿ�С�ڳ�������ʾ�ĵ�һ���������ÿ��Ƿ񳬹��˳���������ʾ�������������
        panic("trying to free block not in datazone");
    bh = get_hash_table(dev,block);
    if (bh) {
        if (bh->b_count != 1) {	//������bh�ǹ���ģ��Ͳ��ͷ�
            printk("trying to free block (%04x:%d), count=%d\n",
                   dev,block,bh->b_count);
            return;
        }
        bh->b_dirt=0;
        bh->b_uptodate=0;
        brelse(bh);		//�ͷŴ�bh
    }
    block -= sb->s_firstdatazone - 1 ;		//��ʱ������ڳ������������򣬼��ڳ������еĵڼ�������
    if (clear_bit(block&8191,sb->s_zmap[block/8192]->b_data)) {	//���Ը���Կ�λ���ڳ������е�����ӳ���б�ʾ������λ�Ƿ�Ϊ1������ӳ���������λ������ʾ�ÿ鲻���ڴ������MAP��
        printk("block (%04x:%d) ",dev,block+sb->s_firstdatazone-1);
        panic("free_block: bit already cleared");
    }
    sb->s_zmap[block/8192]->b_dirt = 1;	//��ʾ���޸��˳���������ӳ�����Ҫд��
}

int new_block(int dev)
{
    struct buffer_head * bh;
    struct super_block * sb;
    int i,j;

    if (!(sb = get_super(dev)))		//ȡ�ø��豸�ļ��ĳ�����
        panic("trying to get new block from nonexistant device");
    j = 8192;				//8192==8K
    for (i=0 ; i<8 ; i++)
        if (bh=sb->s_zmap[i])
            if ((j=find_first_zero(bh->b_data))<8192)		//��s_zmap��Ѱ��һ���յı���
                break;
    if (i>=8 || !bh || j>=8192)		//δ�ҵ��Ϸ��ı������0
        return 0;
    if (set_bit(j,bh->b_data))		//���������Ѱ�ҵ��Ŀ�λ����ʶ���ѷ���
        panic("new_block: bit already set");
    bh->b_dirt = 1;			//��Ҫд��
    j += i*8192 + sb->s_firstdatazone-1;	//������˿��ڴ����е�ʵ��λ��
    if (j >= sb->s_nzones)		//���ܳ���������ķ�Χ
        return 0;
    if (!(bh=getblk(dev,j)))		//����һ��bufferΪ���dev�ļ��¿�
        panic("new_block: cannot get block");
    if (bh->b_count != 1)
        panic("new block: count is != 1");
    clear_block(bh->b_data);		//����buffer��������0
    bh->b_uptodate = 1;
    bh->b_dirt = 1;
    brelse(bh);				//�ͷŸ�BUFFER
    return j;
}

void free_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;

    if (!inode)
        return;
    if (!inode->i_dev) {			//�������һ��EMPTY INODE
        memset(inode,0,sizeof(*inode));	//���˽ڵ�ĸ�λ��ʼ��Ϊ0
        return;
    }
    if (inode->i_count>1) {		//����ǹ���ڵ㣬����
        printk("trying to free inode with count=%d\n",inode->i_count);
        panic("free_inode");
    }
    if (inode->i_nlinks)
        panic("trying to free inode with links");
    if (!(sb = get_super(inode->i_dev)))
        panic("trying to free inode on nonexistent device");
    if (inode->i_num < 1 || inode->i_num > sb->s_ninodes)
        panic("trying to free inode 0 or nonexistant inode");
    if (!(bh=sb->s_imap[inode->i_num>>13]))		//����8K
        panic("nonexistent imap in superblock");
    if (clear_bit(inode->i_num&8191,bh->b_data))	//��s_imap��������úŵĽڵ�Ӱ��
        panic("free_inode: bit already cleared");
    bh->b_dirt = 1;			//Ҫд��
    memset(inode,0,sizeof(*inode));
}

struct m_inode * new_inode(int dev)
{
    struct m_inode * inode;
    struct super_block * sb;
    struct buffer_head * bh;
    int i,j;

    if (!(inode=get_empty_inode()))	//���ڴ�ڵ㻺������ȡһ�����еĽڵ�
        return NULL;
    if (!(sb = get_super(dev)))		//ȡDEV�ļ��ĳ�����
        panic("new_inode with unknown device");
    j = 8192;				//==8K
    for (i=0 ; i<8 ; i++)
        if (bh=sb->s_imap[i])	//��s_imap�з���һ����λ��
            if ((j=find_first_zero(bh->b_data))<8192)
                break;
    if (!bh || j >= 8192 || j+i*8192 > sb->s_ninodes) {
        iput(inode);
        return NULL;
    }
    if (set_bit(j,bh->b_data))		//����s_imap�з��ֵĿ�λ����1��Ҳ���Ǳ������
        panic("new_inode: bit already set");
    bh->b_dirt = 1;			//Ҫд��
    inode->i_count=1;
    inode->i_nlinks=1;
    inode->i_dev=dev;
    inode->i_dirt=1;
    inode->i_num = j + i*8192;		//������˽ڵ���ڴ�������Ӧ���
    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
    return inode;
}
