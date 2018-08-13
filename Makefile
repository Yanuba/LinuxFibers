obj-m := FibersLKM.o
PWD := $(shell pwd)
CC = gcc -Wall

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc Fibers.c -lpthread	#Change this
clear:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm a.out	#Change this