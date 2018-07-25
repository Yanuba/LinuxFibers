#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>

#include "FibersLKM.h"

int main() {
    int fd = -1;

    if((fd = open("/dev/FibersLKM", O_RDONLY)) == -1) {
		perror("Error opening special device file");
		exit(EXIT_FAILURE);
	}
	
	printf("I'm process %d\n", getpid()); 

    ioctl(fd, IOCTL_EX);

    return close(fd);
}
