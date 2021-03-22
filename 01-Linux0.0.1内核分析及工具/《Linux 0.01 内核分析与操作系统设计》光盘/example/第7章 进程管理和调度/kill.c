#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <asm/segment.h>

int sys_pause(void);
int sys_close(int fd);

void release(struct task_struct * p)			//释放该任务的资源
{
    int i;

    if (!p)
        return;
    for (i=1 ; i<NR_TASKS ; i++)
        if (task[i]==p) {				//找到该进程的任务号
            task[i]=NULL;
            free_page((long)p);			//释放该任务的页
            schedule();				//重新调度所有任务
            return;
        }
    panic("trying to release non-existent task");
}

static inline void send_sig(long sig,struct task_struct * p,int priv)	//向进程任务传送信号
{
    if (!p || sig<1 || sig>32)
        return;
    if (priv ||
            current->uid==p->uid ||
            current->euid==p->uid ||
            current->uid==p->euid ||
            current->euid==p->euid)
        p->signal |= (1<<(sig-1));			//接收信号其实就是进程结构中表示信号的long中的一位被置位。
}

void do_kill(long pid,long sig,int priv)
{
    struct task_struct **p = NR_TASKS + task;		//指针指向最后一个任务

    if (!pid) while (--p > &FIRST_TASK) {		　　//如果进程号为0（初始进程）
            if (*p && (*p)->pgrp == current->pid)
                send_sig(sig,*p,priv);
        } else if (pid>0) while (--p > &FIRST_TASK) {	//如果进程号不是初始进程号
            if (*p && (*p)->pid == pid)
                send_sig(sig,*p,priv);
        } else if (pid == -1) while (--p > &FIRST_TASK)	//如果PID=-1，那么就给所有的任务发信号
            send_sig(sig,*p,priv);
    else while (--p > &FIRST_TASK)		　　　	//如果PID〈-1
            if (*p && (*p)->pgrp == -pid)
                send_sig(sig,*p,priv);
}

int sys_kill(int pid,int sig)
{
    do_kill(pid,sig,!(current->uid || current->euid));
    return 0;
}

int do_exit(long code)
{
    int i;

    free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));	//释放本进程的代码段的页表
    free_page_tables(get_base(current->ldt[2]),get_limit(0x17));	//释放本进程的数据段的页表
    for (i=0 ; i<NR_TASKS ; i++)		//将本进程的子进程的父设为0，那么这些子任务不就成了孤儿？
        if (task[i] && task[i]->father == current->pid)
            task[i]->father = 0;
    for (i=0 ; i<NR_OPEN ; i++)			//关闭该进程打开的所有文件
        if (current->filp[i])
            sys_close(i);		　　　	//在open.c中定义
    iput(current->pwd);				//释放本进程所在目录的INODE
    current->pwd=NULL;
    iput(current->root);				//释放本进程根目录的INODE
    current->root=NULL;
    if (current->leader && current->tty >= 0)		//释放本进程占用的TTY
        tty_table[current->tty].pgrp = 0;
    if (last_task_used_math == current)
        last_task_used_math = NULL;
    if (current->father) {			//如果该进程有父进程
        current->state = TASK_ZOMBIE;
        do_kill(current->father,SIGCHLD,1);	//给相应的任务发信号
        current->exit_code = code;
    } else
        release(current);					//释放当前任务资源
    schedule();
    return (-1);	/* just to suppress warnings */
}

int sys_exit(int error_code)
{
    return do_exit((error_code&0xff)<<8);
}

int sys_waitpid(pid_t pid,int * stat_addr, int options)
{
    int flag=0;
    struct task_struct ** p;

    verify_area(stat_addr,4);
repeat:
    for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
        if (*p && *p != current &&
                (pid==-1 || (*p)->pid==pid ||
                 (pid==0 && (*p)->pgrp==current->pgrp) ||
                 (pid<0 && (*p)->pgrp==-pid)))
            if ((*p)->father == current->pid) {	//如果任务是当前任务的子任务
                flag=1;
                if ((*p)->state==TASK_ZOMBIE) {
                    put_fs_long((*p)->exit_code,
                                (unsigned long *) stat_addr);	//保存本任务的退出码到stat_addr
                    current->cutime += (*p)->utime;	//当前进程的时间加上要释放的子进程时间
                    current->cstime += (*p)->stime;
                    flag = (*p)->pid;
                    release(*p);		//释放子进程
                    return flag;
                }
            }
    if (flag) {
        if (options & WNOHANG)
            return 0;
        sys_pause();				　//暂停当前任务
        if (!(current->signal &= ~(1<<(SIGCHLD-1))))	//如果当前任务没有接收到SIGCHLD信号，循环
            goto repeat;
        else
            return -EINTR;
    }
    return -ECHILD;
}
