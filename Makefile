CC = gcc
CFLAGS = -g -c -Wall -Wvla
AR = ar -rc
RANLIB = ranlib

SCHED = PSJF

all: mypthread.a test

mypthread.a: mypthread.o
	$(AR) libmypthread.a mypthread.o
	$(RANLIB) libmypthread.a

mypthread.o: mypthread.c mypthread.h
	$(CC) -pthread $(CFLAGS) -DLIBRARY_MALLOC mypthread.c

umalloc.o: umalloc.c umalloc.h
	$(CC) $(CFLAGS) umalloc.c


ifeq ($(SCHED), RR)
	$(CC) -pthread $(CFLAGS) mypthread.c
else ifeq ($(SCHED), PSJF)
	$(CC) -pthread $(CFLAGS) -DPSJF mypthread.c
else ifeq ($(SCHED), MLFQ)
	$(CC) -pthread $(CFLAGS) -DMLFQ mypthread.c
else
	echo "no such scheduling algorithm"
endif



test.o: test.c mypthread.h umalloc.h
	$(CC) -c -g test.c -pthread
test: test.o mypthread.o umalloc.o
	$(CC) -o test test.o mypthread.o umalloc.o -pthread 

parallel_cal.o: parallel_cal.c mypthread.h umalloc.h
	$(CC) -c -g parallel_cal.c -pthread 
parallel_cal: parallel_cal.o mypthread.o umalloc.o
	$(CC) -o parallel_cal parallel_cal.o mypthread.o umalloc.o -pthread 


umalloc.o: umalloc.c umalloc.h mypthread.h
	gcc -c -g umalloc.c
memgrind.o: memgrind.c umalloc.h 
	gcc -c -g memgrind.c
mallocTest: mallocTest.o umalloc.o mypthread.o
	gcc -o mallocTest mallocTest.o mypthread.o umalloc.o


clean:
	rm -rf testfile *.o *.a test swapfile*.txt
	
