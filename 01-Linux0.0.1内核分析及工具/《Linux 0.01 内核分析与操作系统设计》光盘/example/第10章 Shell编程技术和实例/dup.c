/* dup.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void DisplayOutput()
{
    fprintf(stdout, "This is to stdout\n");
    fprintf(stderr, "This is to stderr\n");
}
int main(int argc, char **argv)
{
    int _newStdOut, _newStdErr;

    //call the output routine
    DisplayOutput();
    //change the stdout and stderr file descriptors
    _newStdOut = open("out", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
    _newStdErr = open("err", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
    dup2(_newStdOut, 1);
    dup2(_newStdErr, 2);
    //call the output routine again
    DisplayOutput();
    return 0;
}
