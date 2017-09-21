OBJS = main.o
CC = g++
CCFLAGS = -Wall -c -std=c++0x -pthread -ggdb3
LFLAGS = -Wall -pthread

zonestats : $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o zonestats

main.o : parser.hh queue.hh workqueue.hh
	$(CC) $(CCFLAGS) main.cc

clean:
	rm *.o zonestats
