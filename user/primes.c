#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void processor(int *pipeline)
{

    close(pipeline[1]);
    int status;
    int prime;
    int read_len = read(pipeline[0], &prime, sizeof(int));
    if (read_len == 0)
    {
        close(pipeline[0]);
        return;
    }
    printf("prime %d\n", prime);
    int out[2];
    pipe(out);
    if (!fork())
    {
        processor(out);
        exit(0);
    }
    else
    {
        close(out[0]);
        int num;
        while (read(pipeline[0], &num, sizeof(int)) != 0)
        {
            if (num % prime != 0)
            {
                write(out[1], &num, sizeof(num));
            }
        }
        close(out[1]);
        close(pipeline[0]);
        wait(&status);
    }
}

int main(int argc, char *argv[])
{
    int status;
    int pipeline[2];
    pipe(pipeline);
    if (!fork())
    {
        processor(pipeline);
        exit(0);
    }
    else
    {
        close(pipeline[0]);
        int num = 2;
        while (1)
        {
            write(pipeline[1], &num, sizeof(num));
            num++;
            if (num > 35)
            {
                close(pipeline[1]);
                break;
            }
        }
        wait(&status);
    }
    exit(0);
}