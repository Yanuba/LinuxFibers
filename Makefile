obj-m := FibersModule.o
FibersModule-objs := FibersLKM.o Fibers_kioctls.o
PWD := $(shell pwd)
CC = gcc -Wall

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -c Fibers.c	#Change this
	gcc -o test Fibers.o FTest.c -lpthread
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm test