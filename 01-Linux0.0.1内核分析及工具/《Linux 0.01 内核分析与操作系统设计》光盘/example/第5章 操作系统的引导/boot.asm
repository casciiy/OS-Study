; boot.asm
; �Ӵ�������װ������һ�����򣬲���ִ��

[ORG 0]

        jmp 07C0h:start     ; ��ת����07C0

start:
        ; ���öμĴ���
        mov ax, cs
        mov ds, ax
        mov es, ax


reset:                      ; ��������������
        mov ax, 0           ;
        mov dl, 0           ; Drive=0 (=A)
        int 13h             ;
        jc reset            ; ERROR => reset again


read:
        mov ax, 1000h       ; ES:BX = 1000:0000
        mov es, ax          ;
        mov bx, 0           ;

        mov ah, 2           ; ��ȡ�������ݵ���ַES:BX
        mov al, 5           ; ��ȡ5������
        mov ch, 0           ; Cylinder=0
        mov cl, 2           ; Sector=2
        mov dh, 0           ; Head=0
        mov dl, 0           ; Drive=0
        int 13h             ; Read!

        jc read             ; ERROR => Try again


        jmp 1000h:0000      ; ��ת����װ�صĳ��򴦣���ʼִ��


times 510-($-$$) db 0
dw 0AA55h