/* sys.c */
#include <errno.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

int sys_ftime()
{
    return -ENOSYS;
}

int sys_mknod()
{
    return -ENOSYS;
}

int sys_break()
{
    return -ENOSYS;
}

int sys_mount()
{
    return -ENOSYS;
}

int sys_umount()
{
    return -ENOSYS;
}

int sys_ustat(int dev,struct ustat * ubuf)
{
    return -1;
}

int sys_ptrace()
{
    return -ENOSYS;
}

int sys_stty()
{
    return -ENOSYS;
}

int sys_gtty()
{
    return -ENOSYS;
}

int sys_rename()
{
    return -ENOSYS;
}

int sys_prof()
{
    return -ENOSYS;
}

int sys_setgid(int gid)					//设置用户组id
{
    if (current->euid && current->uid)
        if (current->gid==gid || current->sgid==gid)
            current->egid=gid;
        else
            return -EPERM;
    else
        current->gid=current->egid=gid;
    return 0;
}

int sys_acct()
{
    return -ENOSYS;
}

int sys_phys()
{
    return -ENOSYS;
}

int sys_lock()
{
    return -ENOSYS;
}

int sys_mpx()
{
    return -ENOSYS;
}

int sys_ulimit()
{
    return -ENOSYS;
}

int sys_time(long * tloc)				//取得当前时间
{
    int i;

    i = CURRENT_TIME;
    if (tloc) {
        verify_area(tloc,4);
        put_fs_long(i,(unsigned long *)tloc);
    }
    return i;
}

int sys_setuid(int uid)				//设置用户id
{
    if (current->euid && current->uid)
        if (uid==current->uid || current->suid==current->uid)
            current->euid=uid;
        else
            return -EPERM;
    else
        current->euid=current->uid=uid;
    return 0;
}

int sys_stime(long * tptr)
{
    if (current->euid && current->uid)
        return -1;
    startup_time = get_fs_long((unsigned long *)tptr) - jiffies/HZ;
    return 0;
}

int sys_times(struct tms * tbuf)			//取得当前时间的各类时间状态
{
    if (!tbuf)
        return jiffies;
    verify_area(tbuf,sizeof *tbuf);
    put_fs_long(current->utime,(unsigned long *)&tbuf->tms_utime);
    put_fs_long(current->stime,(unsigned long *)&tbuf->tms_stime);
    put_fs_long(current->cutime,(unsigned long *)&tbuf->tms_cutime);
    put_fs_long(current->cstime,(unsigned long *)&tbuf->tms_cstime);
    return jiffies;
}

int sys_brk(unsigned long end_data_seg)
{
    if (end_data_seg >= current->end_code &&
            end_data_seg < current->start_stack - 16384)
        current->brk = end_data_seg;
    return current->brk;
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
int sys_setpgid(int pid, int pgid)			//设置进程组id
{
    int i;

    if (!pid)
        pid = current->pid;
    if (!pgid)
        pgid = pid;
    for (i=0 ; i<NR_TASKS ; i++)
        if (task[i] && task[i]->pid==pid) {	//从任务队列中寻找符合pid进程号的进程
            if (task[i]->leader)
                return -EPERM;
            if (task[i]->session != current->session)
                return -EPERM;
            task[i]->pgrp = pgid;
            return 0;
        }
    return -ESRCH;
}

int sys_getpgrp(void)
{
    return current->pgrp;
}

int sys_setsid(void)						//重新设置当前进程的参数
{
    if (current->uid && current->euid)
        return -EPERM;
    if (current->leader)
        return -EPERM;
    current->leader = 1;					//这为建立当前进程的tty做准备
    current->session = current->pgrp = current->pid;
    current->tty = -1;
    return current->pgrp;
}

int sys_uname(struct utsname * name)			//取得本内核的版本信息
{
    static struct utsname thisname = {
                                         "linux .0","nodename","release ","version ","machine "
                                     };
    int i;

    if (!name) return -1;
    verify_area(name,sizeof *name);
    for(i=0;i<sizeof *name;i++)
        put_fs_byte(((char *) &thisname)[i],i+(char *) name);
    return (0);
}

int sys_umask(int mask)					//设置当前进程的掩码
{
    int old = current->umask;

    current->umask = mask & 0777;
    return (old);
}
