/*****************************************************************************
	File: cmtask.c
	Cooperative multitasking with setjmp() and longjmp()
	Compiled by DJGPP
*****************************************************************************/
#include <setjmp.h>		/* jmp_buf, setjmp(), longjmp() */
#include <stdio.h>		/* puts() */
#include <conio.h>		/* kbhit(), getch() */
#include <pc.h>

#define	JMPBUF_IP	state[0].__eip
#define	JMPBUF_SP	state[0].__esp

#define	NUM_TASKS	2
#define	STACK_SIZE	512


typedef struct
{
	jmp_buf state;
} task_t;

static task_t g_tasks[NUM_TASKS + 1];
/*****************************************************************************
g_tasks[0]				state of first task
	...
g_tasks[NUM_TASKS - 1]	state of last task
g_tasks[NUM_TASKS]		jmp_buf state to return to main()
*****************************************************************************/
static void schedule(void)
{
	static unsigned current = NUM_TASKS;
	unsigned prev;

	prev = current;
/* round-robin switch to next task */
	current++;
	if(current >= NUM_TASKS)
		current = 0;
/* return to main() if key pressed */
	if(kbhit())
		current = NUM_TASKS;

/* save old task state
setjmp() returning nonzero means we came here through hyperspace
from the longjmp() below -- just return */
	if(setjmp(g_tasks[prev].state) != 0)
		return;
	else
		/* load new task state */
		longjmp(g_tasks[current].state, 1);
}
/*****************************************************************************
*****************************************************************************/
#define	WAIT	0xFFFFFL

static void wait(void)
{
	unsigned long wait;

	for(wait = WAIT; wait != 0; wait--)
		/* nothing */;
}
/*****************************************************************************
*****************************************************************************/
static void task0(void)
{
	char *p;

	puts("hello from task 0 ");
	while(1)
	{
		schedule(); /* yield() */
		p = (char *)malloc(100);
		puts("this is task 0... ");
		free(p);

		{
			FILE *fp;
			fp = fopen("cmtask.txt", "a+");
			fprintf(fp,"cooperative multitask demo!\n");
			fclose(fp);
		}
		wait();
	}
}
/*****************************************************************************
*****************************************************************************/
static void task1(void)
{
	puts("\tthis is task 1");
	while(1)
	{
		schedule(); /* yield() */

		puts("\t\t\ttask 1 is runing!");
		wait();
	}
}


/*****************************************************************************
*****************************************************************************/
int main(void)
{
	static char stacks[NUM_TASKS][STACK_SIZE];
	volatile unsigned i;
/**/
	unsigned adr;

	for(i = 0; i < NUM_TASKS; i++)
	{
/* set all registers of task state */
		(void)setjmp(g_tasks[i].state);
		adr = (unsigned)(stacks[i] + STACK_SIZE);
/* set SP of task state */
		g_tasks[i].JMPBUF_SP = adr;
	}

/* set IP of task state */
	g_tasks[0].JMPBUF_IP = (unsigned)task0;
	g_tasks[1].JMPBUF_IP = (unsigned)task1;

/* this does not return until a key is pressed */
	schedule();

/* eat keystroke */
	if(getch() == 0)
		(void)getch();
	return 0;
}
