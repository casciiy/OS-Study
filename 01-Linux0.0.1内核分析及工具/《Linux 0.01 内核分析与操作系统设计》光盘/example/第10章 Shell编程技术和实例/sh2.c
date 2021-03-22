/* sh2.c */
#include <unistd.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/wait.h> 
#include <errno.h> 

char *parse_cmd(char *cmd, char **argarr, int *narg) {
    enum states { S_START, S_IN_TOKEN, S_IN_QUOTES, S_SEMI };
    int argc=0; // Arg count
    int loop=1; // Loop control
    enum states state = S_START; // Current state.
    int lastch; // Last character encountered.

    while (loop) {
        switch (state) {
        case S_START:
            if (*cmd == '"') {
                *argarr++ = cmd + 1;
                ++argc;
                state = S_IN_QUOTES;
            }
            else if (*cmd == 0 || *cmd == ';')
                loop = 0;
            else if (*cmd <= ' ')
                *cmd = 0;
            else {
                *argarr++ = cmd;
                ++argc;
                state = S_IN_TOKEN;
            }
            break;
        case S_IN_TOKEN:
            if (*cmd == 0 || *cmd == ';')
                loop = 0;
            else if (*cmd <= ' ') {
                *cmd=0;
                state=S_START;
            }
            break;
        case S_IN_QUOTES:
            if (*cmd == 0)
                loop = 0;
            else if (*cmd == '"') {
                *cmd = 0;
                state = S_START;
            }
        }
        cmd++;
    }
    *argarr = NULL;  /* store the NULL pointer */

    /* Return argument count, if needed. */
    if(narg != NULL) *narg = argc;

    lastch = cmd[-1];
    cmd[-1] = 0;
    return lastch == ';' ? cmd : NULL;
}

int main(int argc, char **argv) {
    char cmd[80]; // Input command area.
    char *source = NULL; // Where commands to send to parser come from.
    char *arg[21]; // Arg list.
    int statval; // Exec status value.
    char *prompt="baby:"; // Command prompt, with default.
    char *test_env; // getenv return value.
    int numargs; // parse_cmd return value.

    if (test_env=getenv("BSPROMPT"))
        prompt=test_env;
    while (1) {
        // See if we need to get a line of input.
        if(source == NULL) {
            printf(prompt);
            source = gets(cmd);
            if(source == NULL) exit(0);  // gets rtns NULL at EOF.
        }

        // Get the next command.
        source = parse_cmd(source, arg, &numargs);
        if (numargs == 0) continue;

        // Exit command
        if (!strcmp(arg[0],"exit")) {
            if (numargs==1)
                exit(0);
            if (numargs==2) {
                if (sscanf(arg[1],"%d",&statval)!=1) {
                    fprintf(stderr,"%s: exit requires an "
                            "integer status code\n",
                            argv[0]);
                    continue;
                }
                exit(statval);
            }
            fprintf(stderr,"%s: exit takes one "
                    "optional parameter -integer status\n",
                    argv[0]);
            continue;
        }

        // Run it.
        if (fork()==0) {
            execvp(arg[0],arg);
            fprintf(stderr,"%s: EXEC of %s failed: %s\n",
                    argv[0],arg[0],strerror(errno));
            exit(1);
        }
        wait(&statval);
        if (WIFEXITED(statval)) {
            if (WEXITSTATUS(statval))
                fprintf(stderr,"%s: child exited with "
                        "status %d\n", argv[0],
                        WEXITSTATUS(statval));
        } else {
            fprintf(stderr,"%s: child died unexpectedly\n",
                    argv[0]);
        }
    }
}
