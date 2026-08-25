CRT= `pwd`
INC= -I$(CRT)/datacenter -I$(CRT)

CC=g++
CFLAGS = -Wall -g -O3 $(INC)

OBJS=eventlist.o pipe.o queue.o clock.o config.o network.o rlb.o rlbmodule.o ndp.o ndppacket.o rlbpacket.o compositequeue.o fairpullqueue.o route.o 

all:	lib 

lib:	$(OBJS) 
	ar -rcs libhtsim.a $(OBJS)

clean:	
	rm -f *.o htsim htsim_* libhtsim.a

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@
