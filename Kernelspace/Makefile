EXTRA_CFLAGS := -I$(PWD)/include
obj-m := FibersModule.o
FibersModule-objs := src/FibersLKM.o src/Fibers_kioctls.o src/proc.o
PWD := $(shell pwd)
CC = gcc -Wall

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
