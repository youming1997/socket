#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    int fd = -1;
    char filename[] = "test.txt";
    fd = creat(filename, 0666);
    if(fd == -1)
        printf("fail to open file %s.\n", filename);
    else
        printf("create file %s successful.\n", filename);

    return 0;
}
