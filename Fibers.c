#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>

#include "FibersLKM.h"
#include "Fibers.h"

void ConvertThreadToFiber() {
    int fd = open("/dev/FibersLKM", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return;
    }
    ioctl(fd, IOCTL_CONVERT);
    close(fd);
    return;
}

int main() {
    int fd = -1;

    if((fd = open("/dev/FibersLKM", O_RDONLY)) == -1) {
		perror("Error opening special device file");
		exit(EXIT_FAILURE);
	}
	
	printf("I'm process %d\n", getpid()); 

    ioctl(fd, IOCTL_EX);

    ConvertThreadToFiber();
    ConvertThreadToFiber();

    return close(fd);
}
