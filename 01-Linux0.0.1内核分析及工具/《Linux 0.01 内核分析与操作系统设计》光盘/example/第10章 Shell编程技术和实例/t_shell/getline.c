getline(void)
{
    int i;

    for (i = 0; (line[i] = getchar())!='\n' && i<MAXLINE; ++i);

    if (i==MAXLINE)
    {
        fprintf(stderr, "Command line too long\n");
        return(ERROR);
    }

    line[i+1] = '\0';
    return(OKAY);
}

