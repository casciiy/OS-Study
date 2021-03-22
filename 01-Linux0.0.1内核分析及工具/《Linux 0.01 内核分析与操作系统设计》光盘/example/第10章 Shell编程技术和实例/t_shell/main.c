#include "def.h"

main(void)
{
    int i;

    initcold();

    for (;;)
    {
        initwarm();

        if (getline())
            if (i = parse())
                execute(i);
    }
}

