CC = gcc
SRC = src/Fibers.c
CFLAGS = -O3 -Wall
OBJS := $(SRC:%.c=%.o)
INCLUDE = -Iinclude -I../Kernelspace/include

all: $(OBJS)

$(OBJS):
	$(CC) $(CFLAGS) $(INCLUDE) -c $(SRC)

.PHONY: clean
clean:
	-rm *.o
