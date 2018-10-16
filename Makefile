obj-m := FibersModule.o
FibersModule-objs := FibersLKM.o Fibers_kioctls.o
PWD := $(shell pwd)
CC = gcc -Wall

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc Fibers.c -lpthread	#Change this
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm a.out	#Change this