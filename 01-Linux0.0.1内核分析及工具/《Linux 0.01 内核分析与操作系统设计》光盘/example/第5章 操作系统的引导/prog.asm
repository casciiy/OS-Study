; prog.ASM
; ����������װ�صĳ���

; ����Ļ�ϴ�ӡ��Program Loaded Succeed! Hello, myos!

[ORG 0]
	jmp start2	; Goto segment 07C0

; ������Ҫ��ӡ���ַ���
msg     db  'Program Loaded Succeed! Hello, myos!',$0

start2:
        ; ���öμĴ���
        mov ax, cs
        mov ds, ax
        mov es, ax

        mov si, msg     ; ��ӡ�ַ���
print:
        lodsb           ; AL=�ַ��������DS:SI

        cmp al, 0       ; If AL=0 then hang
        je hang

        mov ah, 0Eh     ; Print AL
        mov bx, 7
        int 10h

        jmp print       ; ��ӡ��һ���ַ�
hang:                   ; ��������!
	jmp hang

times 510-($-$$) db 0
dw 0AA55h