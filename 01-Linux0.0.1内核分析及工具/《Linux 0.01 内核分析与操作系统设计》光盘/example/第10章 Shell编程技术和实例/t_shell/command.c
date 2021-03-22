command(int i)
{
    int j, flag, inword;

    for (j = 0; j<MAXARG-1; ++j)
    {
        while (*lineptr==' ' || *lineptr=='\t')
            ++lineptr;

        cmdlin[i].av[j] = avptr;
        cmdlin[i].av[j+1] = NULL;

        for (flag = 0; flag==0;)
        {
            switch (*lineptr)
            {
            case '>':
            case '<':
            case '|':
            case '&':
            case '\n':
                if (inword==FALSE)
                    cmdlin[i].av[j] = NULL;

                *avptr++ = '\0';
                return;

            case ' ':
            case '\t':
                inword = FALSE;
                *avptr++ = '\0';
                flag = 1;
                break;

            default:
                inword = TRUE;
                *avptr++ = *lineptr++;
                break;
            }
        }
    }
}

