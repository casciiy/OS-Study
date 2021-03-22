; hello.asm
org      	100h
section  	.text
start:   	mov    ah,9
mov    	dx,szoveg
int 		21h
ret

section  	.data
szoveg   	db "hello world!$"

section .bss
