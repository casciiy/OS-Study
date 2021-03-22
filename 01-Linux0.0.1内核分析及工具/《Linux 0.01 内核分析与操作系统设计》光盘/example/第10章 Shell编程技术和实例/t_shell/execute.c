execute(int j)
{
    int i, fd, fds[2];

    /* 1 */
    if (infile[0]!='\0')
        cmdlin[0].infd = open(infile, O_RDONLY);

    /* 2 */
    if (outfile[0]!='\0')
        if (append==FALSE)
            cmdlin[j-1].outfd = open(outfile, O_WRONLY | O_CREAT
                                     | O_TRUNC, 0666);
        else
            cmdlin[j-1].outfd = open(outfile, O_WRONLY | O_CREAT
                                     | O_APPEND, 0666);

    /* 3 */
    if (backgnd==TRUE)
        signal(SIGCHLD, SIG_IGN);
    else
        signal(SIGCHLD, SIG_DFL);

    /* 4 */
    for (i = 0; i<j; ++i)
    {
        /* 5 */
        if (i<j-1)
        {
            pipe(fds);
            cmdlin[i].outfd = fds[1];
            cmdlin[i+1].infd = fds[0];
        }

        /* 6 */
        forkexec(&cmdlin[i]);

        /* 7 */
        if ((fd = cmdlin[i].infd)!=0)
            close(fd);

        if ((fd = cmdlin[i].outfd)!=1)
            close(fd);
    }

    /* 8 */
    if (backgnd==FALSE)
        while (wait(NULL)!=lastpid);
}

