forkexec(struct cmd *ptr)
{
    int i,pid;

    /* 1 */
    if (pid = fork())
    {
        /* 2 */
        if (backgnd==TRUE)
            printf("%d\n", pid);

        lastpid = pid;
    }
    else
    {
        /* 3 */
        if (ptr->infd==0 && backgnd==TRUE)
            ptr->infd = open("/dev/null", O_RDONLY);

        /* 4 */
        if (ptr->infd!=0)
        {
            close(0);
            dup(ptr->infd);
        }

        if (ptr->outfd!=1)
        {
            close(1);
            dup(ptr->outfd);
        }

        /* 5 */
        if (backgnd==FALSE)
        {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
        }

        /* 6 */
        for (i = 3; i<OPEN_MAX; ++i)
            close(i);

        /* 7 */
        execvp(ptr->av[0], ptr->av);
        exit(1);
    }
}

