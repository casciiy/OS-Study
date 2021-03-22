; prog.ASM
; 被引导程序装载的程序

; 在屏幕上打印：Program Loaded Succeed! Hello, myos!

[ORG 0]
	jmp start2	; Goto segment 07C0

; 定义需要打印的字符串
msg     db  'Program Loaded Succeed! Hello, myos!',$0

start2:
        ; 设置段寄存器
        mov ax, cs
        mov ds, ax
        mov es, ax

        mov si, msg     ; 打印字符串
print:
        lodsb           ; AL=字符串存放在DS:SI

        cmp al, 0       ; If AL=0 then hang
        je hang

        mov ah, 0Eh     ; Print AL
        mov bx, 7
        int 10h

        jmp print       ; 打印下一个字符
hang:                   ; 挂起计算机!
	jmp hang

times 510-($-$$) db 0
dw 0AA55h