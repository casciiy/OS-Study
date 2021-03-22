initcold(void)
{
    /*
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
    */
}


initwarm(void)
{
    int i;

    backgnd = FALSE;
    lineptr = line;
    avptr = avline;
    infile[0] = '\0';
    outfile[0] = '\0';
    append = FALSE;

    for (i = 0; i<PIPELINE; ++i)
    {
        cmdlin[i].infd = 0;
        cmdlin[i].outfd = 1;
    }

    for (i = 3; i<OPEN_MAX; ++i)
        close(i);

    printf("tsh: ");
    fflush(stdout);
}

