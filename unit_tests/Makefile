CC = gcc
INCLUDE = ../umalloc.o ../mypthread.o Unity/src/unity.c
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

all: test_phase_d_3 test_phase_d_2 test_phase_d_1 test_phase_c_2 test_phase_c_1 test_phase_b test_phase_a

test_phase_d_3: test_phase_d_3.c
	$(CC) -o $@ $^ $(INCLUDE)

test_phase_d_2: test_phase_d_2.c
	$(CC) -o $@ $^ $(INCLUDE)

test_phase_d_1: test_phase_d_1.c
	$(CC) -o $@ $^ $(INCLUDE)

test_phase_c_2: test_phase_c_2.c
	$(CC) -o $@ $^ $(INCLUDE)

test_phase_c_1: test_phase_c_1.c
	$(CC) -o $@ $^ $(INCLUDE)

test_phase_b: test_phase_b.c
	$(CC) -o $@ $^ $(INCLUDE)

test_phase_a: test_phase_a.c
	$(CC) -o $@ $^ $(INCLUDE)

clean:
	rm -f test_phase_d_3 test_phase_d_2 test_phase_d_1 test_phase_c_2 test_phase_c_1 test_phase_b test_phase_a swapfile*.txt
