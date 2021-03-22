getname(char *name)
{
    int i;

    for (i = 0; i<MAXNAME; ++i)
    {
        switch (*lineptr)
        {
        case '>':
        case '<':
        case '|':
        case '&':
        case ' ':
        case '\n':
        case '\t':
            *name = '\0';
            return;

        default:
            *name++ = *lineptr++;
            break;
        }
    }

    *name = '\0';
}

