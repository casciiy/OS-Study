; hang.asm
; A minimal bootstrap

hang:					; Hang!
	jmp	hang

	times	510-($-$$)	db 0	; Fill the file with 0's
	dw	0AA55h				; End the file with AA55
