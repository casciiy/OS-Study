#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <asm/segment.h>

int sys_pause(void);
int sys_close(int fd);

void release(struct task_struct * p)			//�ͷŸ��������Դ
{
    int i;

    if (!p)
        return;
    for (i=1 ; i<NR_TASKS ; i++)
        if (task[i]==p) {				//�ҵ��ý��̵������
            task[i]=NULL;
            free_page((long)p);			//�ͷŸ������ҳ
            schedule();				//���µ�����������
            return;
        }
    panic("trying to release non-existent task");
}

static inline void send_sig(long sig,struct task_struct * p,int priv)	//������������ź�
{
    if (!p || sig<1 || sig>32)
        return;
    if (priv ||
            current->uid==p->uid ||
            current->euid==p->uid ||
            current->uid==p->euid ||
            current->euid==p->euid)
        p->signal |= (1<<(sig-1));			//�����ź���ʵ���ǽ��̽ṹ�б�ʾ�źŵ�long�е�һλ����λ��
}

void do_kill(long pid,long sig,int priv)
{
    struct task_struct **p = NR_TASKS + task;		//ָ��ָ�����һ������

    if (!pid) while (--p > &FIRST_TASK) {		����//������̺�Ϊ0����ʼ���̣�
            if (*p && (*p)->pgrp == current->pid)
                send_sig(sig,*p,priv);
        } else if (pid>0) while (--p > &FIRST_TASK) {	//������̺Ų��ǳ�ʼ���̺�
            if (*p && (*p)->pid == pid)
                send_sig(sig,*p,priv);
        } else if (pid == -1) while (--p > &FIRST_TASK)	//���PID=-1����ô�͸����е������ź�
            send_sig(sig,*p,priv);
    else while (--p > &FIRST_TASK)		������	//���PID��-1
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

    free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));	//�ͷű����̵Ĵ���ε�ҳ��
    free_page_tables(get_base(current->ldt[2]),get_limit(0x17));	//�ͷű����̵����ݶε�ҳ��
    for (i=0 ; i<NR_TASKS ; i++)		//�������̵��ӽ��̵ĸ���Ϊ0����ô��Щ�����񲻾ͳ��˹¶���
        if (task[i] && task[i]->father == current->pid)
            task[i]->father = 0;
    for (i=0 ; i<NR_OPEN ; i++)			//�رոý��̴򿪵������ļ�
        if (current->filp[i])
            sys_close(i);		������	//��open.c�ж���
    iput(current->pwd);				//�ͷű���������Ŀ¼��INODE
    current->pwd=NULL;
    iput(current->root);				//�ͷű����̸�Ŀ¼��INODE
    current->root=NULL;
    if (current->leader && current->tty >= 0)		//�ͷű�����ռ�õ�TTY
        tty_table[current->tty].pgrp = 0;
    if (last_task_used_math == current)
        last_task_used_math = NULL;
    if (current->father) {			//����ý����и�����
        current->state = TASK_ZOMBIE;
        do_kill(current->father,SIGCHLD,1);	//����Ӧ�������ź�
        current->exit_code = code;
    } else
        release(current);					//�ͷŵ�ǰ������Դ
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
            if ((*p)->father == current->pid) {	//��������ǵ�ǰ�����������
                flag=1;
                if ((*p)->state==TASK_ZOMBIE) {
                    put_fs_long((*p)->exit_code,
                                (unsigned long *) stat_addr);	//���汾������˳��뵽stat_addr
                    current->cutime += (*p)->utime;	//��ǰ���̵�ʱ�����Ҫ�ͷŵ��ӽ���ʱ��
                    current->cstime += (*p)->stime;
                    flag = (*p)->pid;
                    release(*p);		//�ͷ��ӽ���
                    return flag;
                }
            }
    if (flag) {
        if (options & WNOHANG)
            return 0;
        sys_pause();				��//��ͣ��ǰ����
        if (!(current->signal &= ~(1<<(SIGCHLD-1))))	//�����ǰ����û�н��յ�SIGCHLD�źţ�ѭ��
            goto repeat;
        else
            return -EINTR;
    }
    return -ECHILD;
}
