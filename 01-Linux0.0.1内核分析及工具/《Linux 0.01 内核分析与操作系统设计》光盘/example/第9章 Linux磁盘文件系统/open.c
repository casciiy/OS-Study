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

int sys_utime(char * filename, struct utimbuf * times)		//��times��ʱ�����ø�filename�ļ�
{
    struct m_inode * inode;
    long actime,modtime;

    if (!(inode=namei(filename)))				//ȡ���ļ���inode
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

int sys_access(const char * filename,int mode)			//��modeģʽ�Ƿ�ɷ���filename��ʾ���ļ�
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

int sys_chdir(const char * filename)				//����ǰ���̵�pwdָ��filename�����Ŀ¼
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

int sys_chroot(const char * filename)				//����ǰ���̵�rootָ��filename�����Ŀ¼
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

int sys_chmod(const char * filename,int mode)			//��ָ��filename���ļ���Ŀ¼��mode�滻�ɲ���mode��ģʽ
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

int sys_chown(const char * filename,int uid,int gid)		//��ָ����filename���û���ʶ���û����ʶ�ı��ָ����uid��gid
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
    for(fd=0 ; fd<NR_OPEN ; fd++)				//ȡ��һ����ǰ����δ�õ�filp��׼��ָ����ļ�
        if (!current->filp[fd])
            break;
    if (fd>=NR_OPEN)
        return -EINVAL;
    current->close_on_exec &= ~(1<<fd);			//��close_on_exec�б�Ǹ�λ��ռ��
    f=0+file_table;						//��ָ��ָ���ļ���Ŀ�ʼ
    for (i=0 ; i<NR_FILE ; i++,f++)				//ȡ��һ��δ�õ�file_table����
        if (!f->f_count) break;
    if (i>=NR_FILE)
        return -EINVAL;
    (current->filp[fd]=f)->f_count++;				//�������
    if ((i=open_namei(filename,flag,mode,&inode))<0) {	//�����ļ��������ļ���inode
        current->filp[fd]=NULL;
        f->f_count=0;
        return i;
    }
    /* ttys are somewhat special (ttyxx major==4, tty major==5) */
    if (S_ISCHR(inode->i_mode))
        if (MAJOR(inode->i_zone[0])==4) {			//�����ttyxx�豸�ļ����޸ĵ�ǰ���̵�tty����tty�Ǽ���tty_table
            if (current->leader && current->tty<0) {
                current->tty = MINOR(inode->i_zone[0]);
                tty_table[current->tty].pgrp = current->pgrp;
            }
        } else if (MAJOR(inode->i_zone[0])==5)		//��tty�豸�ļ�
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

    if (fd >= NR_OPEN)					//һ���������ֻ�ܴ�20���ļ�
        return -EINVAL;
    current->close_on_exec &= ~(1<<fd);		//���θ��ļ���close_on_exec�е�ռλ
    if (!(filp = current->filp[fd]))
        return -EINVAL;
    current->filp[fd] = NULL;				//��ý���ָ��ĸ��ļ�ָ��Ϊ��
    if (filp->f_count == 0)
        panic("Close: file count is 0");
    if (--filp->f_count)					//��������������ø��ļ�����ֱ�ӷ���
        return (0);
    iput(filp->f_inode);					//�ͷŸ��ļ���inode
    return (0);
}
