; JUMP.ASM
; Make a jump and then hang

; Tell the compiler that this is offset 0.
; It isn't offset 0, but it will be after the jump.
[ORG 0]

jmp 07C0h:start			; Goto segment 07C0

start:

	; Update the segment registers
	mov ax, cs
	mov ds, ax
	mov es, ax

hang:				; Hang!
	jmp hang

	times 510-($-$$) db 0
	dw 0AA55h