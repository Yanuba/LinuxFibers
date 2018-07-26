obj-m := FibersLKM.o
PWD := $(shell pwd)
CC = gcc -Wall

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clear:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean