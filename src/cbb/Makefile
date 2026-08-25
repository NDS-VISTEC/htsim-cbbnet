CRT= `pwd`
INC= -I$(CRT)/datacenter -I$(CRT)

CC=g++
CFLAGS = -Wall -g -O3 $(INC)
DEPFLAGS = -MMD -MP

OBJS=eventlist.o pipe.o queue.o clock.o config.o cbb_logging.o network.o rlb.o rlbmodule.o ndp.o ndppacket.o rlbpacket.o compositequeue.o fairpullqueue.o route.o controller.o 
DEPS=$(OBJS:.o=.d)

all:	lib 

lib:	$(OBJS) 
	ar -rcs libhtsim.a $(OBJS)

clean:	
	rm -f *.o *.d htsim htsim_* libhtsim.a

.cpp.o:
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEPS)
