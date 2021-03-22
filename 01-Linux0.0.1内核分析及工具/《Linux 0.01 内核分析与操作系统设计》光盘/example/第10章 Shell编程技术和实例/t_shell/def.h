#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0
#define OKAY 1
#define ERROR 0
#define MAXLINE 200                 /* Maximum length of input line */
#define MAXARG 20     /* Max number of args for each simple command */
#define PIPELINE 5   /* Max number of simple commands in a pipeline */
#define MAXNAME 100   /* Maximum length of i/o redirection filename */

char line[MAXLINE+1];                      /* User typed input line */
char *lineptr;             /* Pointer to current position in line[] */
char avline[MAXLINE+1];           /* Argv strings taken from line[] */
char *avptr;             /* Pointer to current position in avline[] */
char infile[MAXNAME+1];               /* Input redirection filename */
char outfile[MAXNAME+1];              /* Ouput redirection filename */

int backgnd;                  /* TRUE if & ends pipeline else FALSE */
int lastpid;              /* PID of last simple command in pipeline */
int append;          /* TRUE for append redirection (>>) else FALSE */

struct cmd
{
    char *av[MAXARG];
    int infd;
    int outfd;
} cmdlin[PIPELINE];        /* Argvs and fds, one per simple command */

