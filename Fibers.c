#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>

#include "FibersLKM.h"
#include "Fibers.h"

void *ConvertThreadToFiber() {
    int ret;
    int fd = open("/dev/FibersLKM", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return;
    }

    struct fiber_struct ctx;

    ret = ioctl(fd, IOCTL_CONVERT, &ctx);
    
    //check ret value
    
    close(fd);
    return ctx;
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
