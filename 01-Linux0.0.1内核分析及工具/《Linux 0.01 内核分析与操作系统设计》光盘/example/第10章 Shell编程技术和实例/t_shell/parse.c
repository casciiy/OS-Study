parse(void)
{
    int i;

    /* 1 */
    command(0);

    /* 2 */
    if (check("<"))
        getname(infile);

    /* 3 */
    for (i = 1; i<PIPELINE; ++i)
        if (check("|"))
            command(i);
        else
            break;

    /* 4 */
    if (check(">"))
    {
        if (check(">"))
            append = TRUE;

        getname(outfile);
    }

    /* 5 */
    if (check("&"))
        backgnd = TRUE;

    /* 6 */
    if (check("\n"))
        return(i);
    else
    {
        fprintf(stderr, "Command line syntax error\n");
        return(ERROR);
    }
}

