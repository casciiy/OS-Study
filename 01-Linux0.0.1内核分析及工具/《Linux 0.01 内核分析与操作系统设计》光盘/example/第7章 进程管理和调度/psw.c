#define FILED		int
#define WINAPI
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <pthread.h>

int sema;

int Put(FILED);
int Get(FILED);

#include <stdio.h>
#include <ctype.h>

#define equal	!strcmp
#define MAXCOUNT	100000

void tstart(void);
void tend(void);
double tval(void);

void tstart2(void);
void tend2(void);
double tval2(void);

//
// cspipe [maxcount]
//
// This program does pipes and threads.
//
// We create two execution environments and pass a token back and forth
// between them as fast as we can. We count the number and time and
// produce a context switches per second number.
//


void USAGE();
int do_threads();
size_t atoik(char *s);

unsigned long maxcount = MAXCOUNT;

unsigned long completedA, completedC;

unsigned long counter = 0;

char *applname;
char applnamebuf[256];
char *pipeAdult = "\\\\.\\pipe\\cspipe";

unsigned long adultId;		// Thread ID's
unsigned long childId;

int main(int ac, char *av[])
{
	int ret = 0;

	strcpy(applnamebuf,av[0]);

	applname = applnamebuf;

	if(ac > 1 && isdigit(av[1][0])) {
		maxcount = atoik(av[1]);
		ac--;
		av++;
		if(maxcount == 0)
			maxcount = 1;
	}
	ret = do_threads();
	return ret;
}

void USAGE()
{
	printf("%s [maximum count]\n",applname);
	return;
}

unsigned long WINAPI threadrun(void * var)
{
	unsigned i;
	char *p = (char *)var;

	if(equal(p,"A")) {		// Adult

		struct sembuf sop;
		int pipeA, pipeB;
		int i;


		pipeA = pipeAfds[1];
		pipeB = pipeBfds[0];

		tstart();
		//
		// ADULT: Writes the first byte.
		//
		for(i = 0; i < maxcount; i++) {
			counter++;
			if(!Put(pipeA))
					break;
			if(!Get(pipeB))
					break;
		}
		tend();
		double t = tval();

		printf("%d pipe/thread Context switches in %7.3f sec ",
			maxcount, t);

		printf("%7.3f usec/cswitch",
			(t*1e6)/maxcount);
		printf("\n");

		sop.sem_num = 0;
		sop.sem_op  = -1;
		sop.sem_flg = 0;
		if(semop(sema, &sop, 1) == -1) {
			printf("semop failed (waiting for threads): err=%d\n", Errno);
			return 1;
		}

	}
	else {

		struct sembuf sop;
		int pipeA, pipeB;
		int i;

		pipeA = pipeAfds[0];
		pipeB = pipeBfds[1];

		//
		// CHILD:
		// The ALREADY_EXISTS detector will wait for the
		// thing to be incremented. The other guy will start
		// this off.
		//
		// Release pipeC and waitfor pipeA
		//
		tstart2();
		for(i = 0; i < maxcount; i++) {
			if(!Get(pipeA))
					break;
			if(!Put(pipeB))
					break;
		}
		tend2();

		double t = tval2();


		printf("%d pipe/thread Context switches in %7.3f sec ",
			maxcount, t);

		printf("%7.3f usec/cswitch",
			(t*1e6)/maxcount);
		printf("\n");

		sop.sem_num = 0;
		sop.sem_op  = -1;
		sop.sem_flg = 0;
		if(semop(sema, &sop, 1) == -1) {
			printf("semop failed (waiting for threads): err=%d\n", Errno);
			return 1;
		}
	}
	return 0;
}
int Put(FILED fd)
{

	if(write(fd,&counter,sizeof(counter)) != sizeof(counter)) {
		printf("Write in A Failed after %d writes err=%d\n",
				counter, Errno);
		return 0;
	}
	return 1;
}

int Get(FILED fd)
{

	if(read(fd,&counter,sizeof(counter)) != sizeof(counter)) {
		printf("Read in A Failed after %d Reads err=%d\n",
				counter, Errno);
		return 0;
	}
	return 1;
}

int do_threads()
{
	//
	// Have to make a semaphore to allow the parent
	// to behave responsibly toward its to thread children.
	//


	if((sema = semget(IPC_PRIVATE, 1, IPC_CREAT|0777)) == -1) {
		printf("semget failed: err=%d\n", Errno);
		return 0;
	}

	struct sembuf sop;

	//
	// Create and initialize semaphore to 2 so each "threadrun()"
	// can decrement it once.
	//
	sop.sem_num = 0;
	sop.sem_op  = 2;
	sop.sem_flg = 0;
	if(semop(sema, &sop, 1) == -1) {
		printf("semop filed: err=%d\n",Errno);
		return 0;
	}

	if(pipe(pipeAfds)) {
		printf("Pipe() syscall failed, errno=%d\n", Errno);
		return 0;
	}
	if(pipe(pipeBfds)) {
		printf("Pipe() syscall failed, errno=%d\n", Errno);
		return 0;
	}
	pthread_t tA, tB;

#	define DEC	( void *(*)(void*) )
	if(pthread_create(&tA, NULL, DEC  threadrun, (void *)"A")) {
		printf("pthread_create A failed: err=%d\n", Errno);
		return 0;
	}
	if(pthread_create(&tB, NULL, DEC threadrun, (void *)"B")) {
		printf("pthread_create B failed: err=%d\n", Errno);
		return 0;
	}
	//
	// Wait for semaphore to go to zero.
	//
	sop.sem_num = 0;
	sop.sem_op  = 0;
	sop.sem_flg = 0;
	if(semop(sema, &sop, 1) == -1) {
		printf("semop failed (waiting for threads): err=%d\n", Errno);
		return 1;
	}

	return 0;
}

#include <ctype.h>

size_t atoik(char *s)
{
	size_t ret = 0;
	size_t base;

	if(*s == '0') {
		base = 8;
		if(*++s == 'x' || *s == 'X') {
			base = 16;
			s++;
		}
	}
	else
		base = 10;

	for(; isxdigit(*s); s++) {
		if(base == 16)
			if(isalpha(*s))
				ret = base*ret + (toupper(*s) - 'A');
			else
				ret = base*ret + (*s - '0');
		else if(isdigit(*s))
				ret = base*ret + (*s - '0');
		else
			break;
	}
	for(; isalpha(*s); s++) {
		switch(toupper(*s)) {
		case 'K': ret *= 1024; break;
		case 'M': ret *= 1024*1024; break;
		default:
			return ret;
		}
	}
	return ret;
}

static struct timeval _tstart, _tend;
static struct timeval _tstart2, _tend2;

void tstart(void)
{
	gettimeofday(&_tstart, NULL);
}
void tstart2(void)
{
	gettimeofday(&_tstart2, NULL);
}
void tend(void)
{
	gettimeofday(&_tend,NULL);
}
void tend2(void)
{
	gettimeofday(&_tend2,NULL);
}

double tval()
{
	double t1, t2;

	t1 =  (double)_tstart.tv_sec + (double)_tstart.tv_usec/(1000*1000);
	t2 =  (double)_tend.tv_sec + (double)_tend.tv_usec/(1000*1000);
	return t2-t1;
}
double tval2()
{
	double t1, t2;

	t1 =  (double)_tstart2.tv_sec + (double)_tstart2.tv_usec/(1000*1000);
	t2 =  (double)_tend2.tv_sec + (double)_tend2.tv_usec/(1000*1000);
	return t2-t1;
}
