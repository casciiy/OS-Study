check(char *ptr)
{
    char *tptr;

    while (*lineptr==' ')
        lineptr++;

    tptr = lineptr;

    while (*ptr!='\0' && *ptr==*tptr)
    {
        ptr++;
        tptr++;
    }

    if (*ptr!='\0')
        return(FALSE);
    else
    {
        lineptr = tptr;
        return(TRUE);
    }
}

