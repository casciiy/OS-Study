/* Compiled by DJGPP */
#include <stdio.h>
#include <pc.h>

void update_cursor(int row, int col)
{
    USHORT position=(row*80) + col;
    /* cursor LOW port to VGA INDEX register */
    outb(0x3D4, 0x0F);
    outb(0x3D5, (UCHAR)(position&0xFF));
    /* cursor HIGH port to vga INDEX register */
    outb(0x3D4, 0x0E);
    outb(0x3D5, (UCHAR)((position>>8)&0xFF));
}

void main(){
    clrscr();
    update_cursor(10, 10);
}
