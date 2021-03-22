; boot.asm
; 从磁盘扇区装载另外一个程序，并且执行

[ORG 0]

        jmp 07C0h:start     ; 跳转到段07C0

start:
        ; 设置段寄存器
        mov ax, cs
        mov ds, ax
        mov es, ax


reset:                      ; 重置软盘驱动器
        mov ax, 0           ;
        mov dl, 0           ; Drive=0 (=A)
        int 13h             ;
        jc reset            ; ERROR => reset again


read:
        mov ax, 1000h       ; ES:BX = 1000:0000
        mov es, ax          ;
        mov bx, 0           ;

        mov ah, 2           ; 读取磁盘数据到地址ES:BX
        mov al, 5           ; 读取5个扇区
        mov ch, 0           ; Cylinder=0
        mov cl, 2           ; Sector=2
        mov dh, 0           ; Head=0
        mov dl, 0           ; Drive=0
        int 13h             ; Read!

        jc read             ; ERROR => Try again


        jmp 1000h:0000      ; 跳转到被装载的程序处，开始执行


times 510-($-$$) db 0
dw 0AA55h