/* timer.c */
/* ������ʹ��Borland C����Turbo C����*/
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <bios.h>
#include <conio.h>

/* Escape key */
#define VK_ESC 0x11b 
#define TIMER 0x1c /* ʱ���жϵ��жϺ� */

/* �жϴ�������C��C++�еı�ʾ���в�ͬ��
���������_cplusplus���ʾ��C++�����£���������C�����¡� */

int TimerCounter=0; /* ��ʱ������ÿ��������18�� */
/* ָ��ԭ��ʱ���жϴ��������ڵ��жϴ�����ָ��(���) */
void interrupt ( *oldhandler)();

/* �µ�ʱ���жϴ����� */
void interrupt newhandler()
{
    /* increase the global counter */
    TimerCounter++;
    /* call the old routine */
    oldhandler();
}
/* �����µ�ʱ���жϴ������ */
void SetTimer(void interrupt (*IntProc)())
{
    oldhandler=getvect(TIMER);
    disable(); /* �����µ�ʱ���жϴ������ʱ����ֹ�����ж� */
    setvect(TIMER,IntProc);
    enable(); /* �����ж� */
}
/* �ָ�ԭ�е�ʱ���жϴ������ */
void KillTimer()
{
    disable();
    setvect(TIMER,oldhandler);
    enable();
}

void main(void)
{
    int key,time=0;
    SetTimer(newhandler); /* �޸�ʱ���ж� */
    for (;;)
    {
        if (bioskey(1))
        {
            key=bioskey(0);
            if (key==VK_ESC) /* ��escape����ǰ�˳����� */
            {
                printf("User cancel!\n");
                break;
            }
        }
        if (TimerCounter>18) /* 1���Ӵ���һ�� */
        {
            /* �ָ���ʱ���� */
            TimerCounter=0;
            time++;
            printf("%d\n",time);
            if (time==10) /* 10���Ӻ�������� */
            {
                printf("Program terminated normally!\n");
                break;
            }
        }
    }
    KillTimer(); /* �ָ�ʱ���ж� */
}
