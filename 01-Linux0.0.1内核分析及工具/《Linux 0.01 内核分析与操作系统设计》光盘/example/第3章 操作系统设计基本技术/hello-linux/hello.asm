section .text 
global main 

main: 
mov eax,4 		;4�ŵ��� 
mov ebx,1 		;ebx��1��ʾstdout 
mov ecx,msg 		;�ַ������׵�ַ����ecx 
mov edx,14 		;�ַ����ĳ�������edx 
int 80h 			;����ִ� 
mov eax,1 		;1�ŵ��� 
int 80h 			;���� 
msg: 
db "Hello World!",0ah,0dh 
