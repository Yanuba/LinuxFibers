SRC = 	src/calqueue.c \
		src/channel.c \
		src/jmp.S \
		src/main.c \
		src/model.c \
		src/numerical.c \
		src/thread.c \
		src/ult.c
		
CFLAGS = -O3 -I../Userspace/include
EXEC = test
OBJS := $(SRC:%.c=%.o) ../Userspace/Fibers.o
OBJS := $(OBJS:%.S=%.o)
LIBS = -lm -lpthread 

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $(EXEC)

%.o : %.S
	$(CC) $< $(LIBS) -c -o $@

.PHONY: clean
clean:
	-rm $(OBJS)
	-rm $(EXEC)
