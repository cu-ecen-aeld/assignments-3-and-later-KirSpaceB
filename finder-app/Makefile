CC ?= $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -g

all: writer

writer: writer.c
	$(CC) $(CFLAGS) -o writer writer.c

clean:
	rm -f writer *.o
