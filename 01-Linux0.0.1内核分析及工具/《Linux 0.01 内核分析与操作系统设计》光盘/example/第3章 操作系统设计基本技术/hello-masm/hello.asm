; Compiled by MASM

; Say hello to the world
; ÎÄ¼þÃû hello.asm
; ------------------------------------------------
        .model small
        .stack 100h
        .data

Welcome db 13,10,'Hello, world!!',13,10,'$'

        .code
start:
        mov ax,@data
        mov ds,ax
        lea dx,welcome
        mov ah,9
        int 21h

;	end the program
        mov ah,4ch
        int 21h

	end start
; ------------------------------------------------
