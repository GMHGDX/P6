CC      = gcc
CFLAGS  = -Wall
.PHONY: all
all: oss user_proc
oss: oss.o oss.h
	$(CC) -o $@ $^
user_proc: user_proc.o oss.h
	$(CC) -o $@ $^
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c

clean: 
	rm *.o oss user_proc