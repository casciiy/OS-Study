/* sh1.c */
#include <unistd.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/wait.h> 
#include <string.h> 
#include <errno.h> 

#define SHELL_NAME "sh1" 
#define PROMPT_ENVIRONMENT_VARIABLE "PROMPT" 

char *prompt;

int main(int argc, char **argv)
{
    char cmd[80];
    int statval;

    /* Determine prompt value. */
    if ((prompt = getenv(PROMPT_ENVIRONMENT_VARIABLE)) == NULL)
        prompt = SHELL_NAME ":";

    /* Process commands until exit, or death by signal. */
    while (1)
    {
        /* Prompt and read a command. */
        printf(prompt);
        gets(cmd);

        /* Process built-in commands. */
        if(strcasecmp(cmd, "exit") == 0)
            break;

        /* Process non-built-in commands. */
        if(fork() == 0) {
            execlp(cmd, cmd, NULL);
            fprintf(stderr, "%s: Exec %s failed: %s\n", argv[0],
                    cmd, strerror(errno));
            exit(1);
        }

        wait(&statval);
        if(WIFEXITED(statval))
        {
            if(WEXITSTATUS(statval))
            {
                fprintf(stderr,
                        "%s: child exited with status %d.\n",
                        argv[0], WEXITSTATUS(statval));
            }
        } else {
            fprintf(stderr, "%s: child died unexpectedly.\n",
                    argv[0]);
        }
    }
}
