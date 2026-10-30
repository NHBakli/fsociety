#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#ifndef MFD_CLOEXEC
#endif

#ifndef MFD_CLOEXEC
# define MFD_CLOEXEC 0x0001U
#endif

int main(void *args){
    char *filename = (char *)args;
    FILE *file = fopen(filename, "rb");

    if (!file) {
        perror("fopen");
        return 1;
    }

    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        perror("open");
        fclose(file);
        return 1;
    }

    read(fd, args, 1024); 

    FILE *memfd = memfd_create("loader", MFD_CLOEXEC);

    fclose(file);
    close(fd);
    return 0;
}