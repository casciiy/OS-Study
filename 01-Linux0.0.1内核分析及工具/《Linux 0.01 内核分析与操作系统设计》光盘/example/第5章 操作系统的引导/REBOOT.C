#include <stdio.h>

void reboot()
{
    asm{
        mov ax,0ffffh
        push ax
        xor ax,ax
        push ax
        retf
    }
}

void main()
{
    printf("Please press any key to reboot...\n");
    getch();
    reboot();
}
