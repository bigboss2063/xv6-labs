#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXARGS 10

int main(int argc, char *argv[])
{
    int buf_idx, read_len;
    int status;
    char buf[512];
    char* args[MAXARGS];

    int i;

    for (i = 1; i < argc; i++) {
        args[i-1] = argv[i];
    }

    while (1)
    {
        buf_idx = 0;
        while (1)
        {
            read_len = read(0, &buf[buf_idx], sizeof(char));
            if (read_len <= 0 || buf[buf_idx] == '\n') {
                break;
            }
            buf_idx++;
        }
        

        if (read_len == 0) {
            break;
        }

        buf[buf_idx] = '\0';
        args[argc-1] = buf;
        if (!fork()) {
            exec(args[0], args);
            exit(0);
        } else {
            wait(&status);
        }
    }

    exit(0);
}
