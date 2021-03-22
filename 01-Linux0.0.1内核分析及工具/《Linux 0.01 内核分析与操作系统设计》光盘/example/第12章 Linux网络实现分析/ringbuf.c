/* ringbuf.c */
#include <stdio.h>
#include <ctype.h>

#define NMAX 8

int iput = 0;  /* ���λ������ĵ�ǰ����λ�� */
int iget = 0;  /* ���λ������ĵ�ǰȡ��λ�� */
int n = 0;     /* ���λ������е�Ԫ�������� */

double buffer[NMAX];

/*
	���λ������ĵ�ַ��ż��㺯����������﻽�ѻ�������β�������ƻص�ͷ����
	���λ���������Ч��ַ���Ϊ��0 �� ��NMAX��1��
*/

int addring(int i)
{
    return (i+1) == NMAX ? 0 : i+1;
}

/* �ӻ��λ�������ȡһ��Ԫ�� */
double get(void)
{
    int pos;

    if (n > 0) {
        pos = iget;
        iget = addring(iget);
        n--;
        return buffer[pos];
    }
    else {
        printf("Buffer is empty\n");
        return 0.0;
    }
}

/* ���λ������з���һ��Ԫ�� */
void put(double z)
{
    if (n < NMAX) {
        buffer[iput] = z;
        iput = addring(iput);
        n++;
    }
    else
        printf("Buffer is full\n");
}


int main(void)
{
    char opera[5];
    double z;

    do {
        printf("Please input p|g|e ?");
        scanf("%s", &opera);

        switch (tolower(opera[0])) {
        case 'p': /* put */
            printf("Please input a float number? ");
            scanf("%lf", &z);
            put(z);
            break;
        case 'g': /* get */
            z = get();
            printf("%8.2f from Buffer\n", z);
            break;
        case 'e':
            printf("End\n");
            break;
        default:
            printf("%s - Operation command error!\n", opera);
        } /* end switch */

    } while (opera[0] != 'e');

    return 0;
}
