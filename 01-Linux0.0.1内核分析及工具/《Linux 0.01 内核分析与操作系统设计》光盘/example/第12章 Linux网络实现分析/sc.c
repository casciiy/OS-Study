/* sc.c */
#include <stdio.h>
#include <bios.h>
#include <conio.h>

#define	COM_INIT	0
#define	COM_SEND	1
#define	COM_RECEIVE	2
#define	COM_STATUS	3

#define COM1       0
#define DATA_READY 0x100
#define TRUE       1
#define FALSE      0

#define SETTINGS ( 0x80 | 0x02 | 0x00 | 0x00)
/* 设置串口为1个停止位，无奇偶校验，1200波特 */

#define PRINT_SEND_CHAR(ch)	printf("send\t\t%c\n", ch)
#define PRINT_RECV_CHAR(ch)	printf("receive\t\t%c\n", ch)

int main(int argc, char *argv)
{
    int status, DONE = FALSE;
    char ch;

    /* 对COM1进行初始化设置 */
    bioscom(COM_INIT, SETTINGS, COM1);

    printf("... BIOSCOM Demo, any key to exit ...\n");
    printf("-= UESTC =-\n");

    if(argc != 1)
    {
        printf("This is client! I will send a char at first :)\n");
        bioscom(COM_SEND, 'a', COM1);
        PRINT_SEND_CHAR('a');
    }

    while (!DONE)
    {
        status = bioscom(COM_STATUS, 0, COM1);
        /* 查询串口状态 */

        if (status & DATA_READY)			/* 有数据到达 */
        {
            if ((ch = bioscom(COM_RECEIVE, 0, COM1) & 0x7F) != 0)
            {
                PRINT_RECV_CHAR(ch);

                if(ch == 'z') ch = 'a';
                else ch = ch + 1;

                bioscom(COM_SEND, ch, COM1);
                PRINT_SEND_CHAR(ch);
            }
        }

        if(kbhit())
        {
            DONE = TRUE;
        }
    }
    return 0;
}
