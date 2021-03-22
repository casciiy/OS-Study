#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

_syscall3(int,write,int,fd,const char *,buf,off_t,count)
//_syscall1(int,MP,const char *,buf)
int MP(const char * fmt,...)
{
	int i;
	va_list arg;
	va_start(arg,fmt);
	__asm__("int $0x80"
		:"=a"(i)
		:"0"(__NR_MP),"b"(fmt),"c"(va_arg(arg,const char *))
		);
	return i;


}
