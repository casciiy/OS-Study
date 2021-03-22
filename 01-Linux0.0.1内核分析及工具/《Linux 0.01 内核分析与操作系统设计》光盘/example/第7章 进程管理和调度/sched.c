/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <signal.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
    struct task_struct task;
    char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

long volatile jiffies=0;
long startup_time=0;
struct task_struct *current = &(init_task.task), *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
    long * a;
    short b;
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()				//保存协处理器的状态参数
{
    if (last_task_used_math)
        __asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
    if (current->used_math)
        __asm__("frstor %0"::"m" (current->tss.i387));
    else {
        __asm__("fninit"::);
        current->used_math=1;
    }
    last_task_used_math=current;
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
    int i,next,c;
    struct task_struct ** p;

    /* check alarm, wake up any interruptible tasks that have got a signal */

    for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
        if (*p) {
            if ((*p)->alarm && (*p)->alarm < jiffies) {
                (*p)->signal |= (1<<(SIGALRM-1));
                (*p)->alarm = 0;
            }
            if ((*p)->signal && (*p)->state==TASK_INTERRUPTIBLE)
                (*p)->state=TASK_RUNNING;
        }

    /* this is the scheduler proper: */

    while (1) {				//此循环是取得任务的counter最高的那个任务，作为要切换的任务
        c = -1;
        next = 0;
        i = NR_TASKS;
        p = &task[NR_TASKS];
        while (--i) {
            if (!*--p)
                continue;
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
                c = (*p)->counter, next = i;
        }
        if (c) break;			//如果所有的任务counter都一样，且都为0，那么执行下面的代码，根据任务的先后顺序和权限大小重设各任务的counter
        for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
            if (*p)
                (*p)->counter = ((*p)->counter >> 1) +
                                (*p)->priority;
    }
    switch_to(next);			//切换任务
}

int sys_pause(void)				//系统暂停
{
    current->state = TASK_INTERRUPTIBLE;
    schedule();
    return 0;
}

void sleep_on(struct task_struct **p)	//进程休眠
{
    struct task_struct *tmp;

    if (!p)
        return;
    if (current == &(init_task.task))		//初始化任务不能休眠
        panic("task[0] trying to sleep");
    tmp = *p;
    *p = current;
    current->state = TASK_UNINTERRUPTIBLE;
    schedule();
    if (tmp)
        tmp->state=0;			//让**p的进程进入运行状态
}

void interruptible_sleep_on(struct task_struct **p)
{
    struct task_struct *tmp;

    if (!p)
        return;
    if (current == &(init_task.task))
        panic("task[0] trying to sleep");
    tmp=*p;
    *p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
    schedule();
    if (*p && *p != current) {
        (**p).state=0;		//先唤醒其他的进程
        goto repeat;
    }
    *p=NULL;
    if (tmp)
        tmp->state=0;			//唤醒指定的进程
}

void wake_up(struct task_struct **p)
{
    if (p && *p) {
        (**p).state=0;
        *p=NULL;
    }
}

void do_timer(long cpl)
{
    if (cpl)
        current->utime++;
    else
        current->stime++;
    if ((--current->counter)>0) return;
    current->counter=0;
    if (!cpl) return;
    schedule();
}

int sys_alarm(long seconds)
{
    current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
    return seconds;
}

int sys_getpid(void)
{
    return current->pid;
}

int sys_getppid(void)
{
    return current->father;
}

int sys_getuid(void)
{
    return current->uid;
}

int sys_geteuid(void)
{
    return current->euid;
}

int sys_getgid(void)
{
    return current->gid;
}

int sys_getegid(void)
{
    return current->egid;
}

int sys_nice(long increment)
{
    if (current->priority-increment>0)
        current->priority -= increment;
    return 0;
}

int sys_signal(long signal,long addr,long restorer)		//信号处理
{
    long i;

    switch (signal) {
case SIGHUP: case SIGINT: case SIGQUIT: case SIGILL:
case SIGTRAP: case SIGABRT: case SIGFPE: case SIGUSR1:
case SIGSEGV: case SIGUSR2: case SIGPIPE: case SIGALRM:
    case SIGCHLD:
        i=(long) current->sig_fn[signal-1];
        current->sig_fn[signal-1] = (fn_ptr) addr;	//此信号处理函数的地址
        current->sig_restorer = (fn_ptr) restorer;
        return i;
    default: return -1;
    }
}

void sched_init(void)
{
    int i;
    struct desc_struct * p;

    //以下2行设置初始任务的TSS和LDT的描述符表
    set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
    set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
    p = gdt+2+FIRST_TSS_ENTRY;			//将指针指向GDT中的TSS1=6
    for(i=1;i<NR_TASKS;i++) {			//初始化63个任务
        task[i] = NULL;
        p->a=p->b=0;				//p->a为TSS（N）的低32位，p->b为TSS（N）的高32位
        p++;
        p->a=p->b=0;				//p->a为LDT（N）的低32位，p->b为LDT（N）的高32位

        p++;
    }
    ltr(0);					//将TSS0（即任务0）装入任务寄存器TR
    lldt(0);					//将LDT0装入LDTR
    outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */	//0x43是系统时钟0的模式控制端口，0x36是指用2进制模式3，先写低8位，再写高8位来显示时间
    //以下2行是将时钟0改变为每秒100次
    outb_p(LATCH & 0xff , 0x40);	/* LSB */
    outb(LATCH >> 8 , 0x40);	/* MSB */
    set_intr_gate(0x20,&timer_interrupt);	//设置时钟中断为20H
    outb(inb_p(0x21)&~0x01,0x21);		//开放IRQ1，系统时钟0中断
    set_system_gate(0x80,&system_call);
}
