#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/segment.h>

extern int rw_char(int rw,int dev, char * buf, int count);
extern int read_pipe(struct m_inode * inode, char * buf, int count);
extern int write_pipe(struct m_inode * inode, char * buf, int count);
extern int block_read(int dev, off_t * pos, char * buf, int count);
extern int block_write(int dev, off_t * pos, char * buf, int count);
extern int file_read(struct m_inode * inode, struct file * filp,
                         char * buf, int count);
extern int file_write(struct m_inode * inode, struct file * filp,
                          char * buf, int count);

int sys_lseek(unsigned int fd,off_t offset, int origin)	//在文件中定位
{
    struct file * file;
    int tmp;

    if (fd >= NR_OPEN || !(file=current->filp[fd]) || !(file->f_inode)
            || !IS_BLOCKDEV(MAJOR(file->f_inode->i_dev)))	//fd的合法性检查
        return -EBADF;
    if (file->f_inode->i_pipe)				//此打开的文件不能是一个管道
        return -ESPIPE;
    switch (origin) {
    case 0:
        if (offset<0) return -EINVAL;
        file->f_pos=offset;			//文件位移重置
        break;
    case 1:
        if (file->f_pos+offset<0) return -EINVAL;
        file->f_pos += offset;		//文件当前位置加上要位移的数
        break;
    case 2:
        if ((tmp=file->f_inode->i_size+offset) < 0)
            return -EINVAL;
        file->f_pos = tmp;			//在文件尾附加数据
        break;
    default:
        return -EINVAL;
    }
    return file->f_pos;
}

int sys_read(unsigned int fd,char * buf,int count)
{
    struct file * file;
    struct m_inode * inode;

    if (fd>=NR_OPEN || count<0 || !(file=current->filp[fd]))	//fd合法性检查
        return -EINVAL;
    if (!count)
        return 0;
    verify_area(buf,count);
    inode = file->f_inode;
    if (inode->i_pipe)					//如果是管道
        return (file->f_mode&1)?read_pipe(inode,buf,count):-1;
    if (S_ISCHR(inode->i_mode))				//如果是字符设备文件
        return rw_char(READ,inode->i_zone[0],buf,count);
    if (S_ISBLK(inode->i_mode))				//如果是块设备文件
        return block_read(inode->i_zone[0],&file->f_pos,buf,count);
    if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {	//如果是目录或正常文件
        if (count+file->f_pos > inode->i_size)
            count = inode->i_size - file->f_pos;
        if (count<=0)
            return 0;
        return file_read(inode,file,buf,count);
    }
    printk("(Read)inode->i_mode=%06o\n\r",inode->i_mode);
    return -EINVAL;
}

int sys_write(unsigned int fd,char * buf,int count)
{
    struct file * file;
    struct m_inode * inode;

    if (fd>=NR_OPEN || count <0 || !(file=current->filp[fd]))
        return -EINVAL;
    if (!count)
        return 0;
    inode=file->f_inode;
    if (inode->i_pipe)					//如果是管道
        return (file->f_mode&2)?write_pipe(inode,buf,count):-1;
    if (S_ISCHR(inode->i_mode))				//如果是字符设备文件
        return rw_char(WRITE,inode->i_zone[0],buf,count);
    if (S_ISBLK(inode->i_mode))				//如果是块设备文件
        return block_write(inode->i_zone[0],&file->f_pos,buf,count);
    if (S_ISREG(inode->i_mode))				//如果是正常文件
        return file_write(inode,file,buf,count);
    printk("(Write)inode->i_mode=%06o\n\r",inode->i_mode);
    return -EINVAL;
}
