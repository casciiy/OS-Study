#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>

int sys_utime(char * filename, struct utimbuf * times)		//将times的时间设置给filename文件
{
    struct m_inode * inode;
    long actime,modtime;

    if (!(inode=namei(filename)))				//取该文件的inode
        return -ENOENT;
    if (times) {
        actime = get_fs_long((unsigned long *) &times->actime);
        modtime = get_fs_long((unsigned long *) &times->modtime);
    } else
        actime = modtime = CURRENT_TIME;
    inode->i_atime = actime;
    inode->i_mtime = modtime;
    inode->i_dirt = 1;
    iput(inode);
    return 0;
}

int sys_access(const char * filename,int mode)			//用mode模式是否可访问filename表示的文件
{
    struct m_inode * inode;
    int res;

    mode &= 0007;
    if (!(inode=namei(filename)))
        return -EACCES;
    res = inode->i_mode & 0777;
    iput(inode);
    if (!(current->euid && current->uid))
        if (res & 0111)
            res = 0777;
        else
            res = 0666;
    if (current->euid == inode->i_uid)
        res >>= 6;
    else if (current->egid == inode->i_gid)
        res >>= 6;
    if ((res & 0007 & mode) == mode)
        return 0;
    return -EACCES;
}

int sys_chdir(const char * filename)				//将当前进程的pwd指向filename代表的目录
{
    struct m_inode * inode;

    if (!(inode = namei(filename)))
        return -ENOENT;
    if (!S_ISDIR(inode->i_mode)) {
        iput(inode);
        return -ENOTDIR;
    }
    iput(current->pwd);
    current->pwd = inode;
    return (0);
}

int sys_chroot(const char * filename)				//将当前进程的root指向filename代表的目录
{
    struct m_inode * inode;

    if (!(inode=namei(filename)))
        return -ENOENT;
    if (!S_ISDIR(inode->i_mode)) {
        iput(inode);
        return -ENOTDIR;
    }
    iput(current->root);
    current->root = inode;
    return (0);
}

int sys_chmod(const char * filename,int mode)			//将指定filename的文件或目录的mode替换成参数mode的模式
{
    struct m_inode * inode;

    if (!(inode=namei(filename)))
        return -ENOENT;
    if (current->uid && current->euid)
        if (current->uid!=inode->i_uid && current->euid!=inode->i_uid) {
            iput(inode);
            return -EACCES;
        } else
            mode = (mode & 0777) | (inode->i_mode & 07000);
    inode->i_mode = (mode & 07777) | (inode->i_mode & ~07777);
    inode->i_dirt = 1;
    iput(inode);
    return 0;
}

int sys_chown(const char * filename,int uid,int gid)		//将指定的filename的用户标识和用户组标识改变成指定的uid和gid
{
    struct m_inode * inode;

    if (!(inode=namei(filename)))
        return -ENOENT;
    if (current->uid && current->euid) {
        iput(inode);
        return -EACCES;
    }
    inode->i_uid=uid;
    inode->i_gid=gid;
    inode->i_dirt=1;
    iput(inode);
    return 0;
}

int sys_open(const char * filename,int flag,int mode)
{
    struct m_inode * inode;
    struct file * f;
    int i,fd;

    mode &= 0777 & ~current->umask;
    for(fd=0 ; fd<NR_OPEN ; fd++)				//取得一个当前进程未用的filp，准备指向该文件
        if (!current->filp[fd])
            break;
    if (fd>=NR_OPEN)
        return -EINVAL;
    current->close_on_exec &= ~(1<<fd);			//在close_on_exec中标记该位已占用
    f=0+file_table;						//将指针指向文件表的开始
    for (i=0 ; i<NR_FILE ; i++,f++)				//取得一个未用的file_table表项
        if (!f->f_count) break;
    if (i>=NR_FILE)
        return -EINVAL;
    (current->filp[fd]=f)->f_count++;				//标记在用
    if ((i=open_namei(filename,flag,mode,&inode))<0) {	//根据文件名来打开文件的inode
        current->filp[fd]=NULL;
        f->f_count=0;
        return i;
    }
    /* ttys are somewhat special (ttyxx major==4, tty major==5) */
    if (S_ISCHR(inode->i_mode))
        if (MAJOR(inode->i_zone[0])==4) {			//如果是ttyxx设备文件，修改当前进程的tty并将tty登记入tty_table
            if (current->leader && current->tty<0) {
                current->tty = MINOR(inode->i_zone[0]);
                tty_table[current->tty].pgrp = current->pgrp;
            }
        } else if (MAJOR(inode->i_zone[0])==5)		//是tty设备文件
            if (current->tty<0) {
                iput(inode);
                current->filp[fd]=NULL;
                f->f_count=0;
                return -EPERM;
            }
    f->f_mode = inode->i_mode;
    f->f_flags = flag;
    f->f_count = 1;
    f->f_inode = inode;
    f->f_pos = 0;
    return (fd);
}

int sys_creat(const char * pathname, int mode)
{
    return sys_open(pathname, O_CREAT | O_TRUNC, mode);
}

int sys_close(unsigned int fd)
{
    struct file * filp;

    if (fd >= NR_OPEN)					//一个进程最多只能打开20个文件
        return -EINVAL;
    current->close_on_exec &= ~(1<<fd);		//屏蔽该文件在close_on_exec中的占位
    if (!(filp = current->filp[fd]))
        return -EINVAL;
    current->filp[fd] = NULL;				//令该进程指向的该文件指针为空
    if (filp->f_count == 0)
        panic("Close: file count is 0");
    if (--filp->f_count)					//如果有其他进程用该文件，就直接返回
        return (0);
    iput(filp->f_inode);					//释放该文件的inode
    return (0);
}
