CC = g++
CFLAGS = -Wall -g -O3 $(INC)
DEPFLAGS = -MMD -MP
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
LIB=-L.. 

OBJS = mainSimulator.o dynamicTopology.o networkDevices.o CBB_Master.o CBB_Maker.o tor.o
DEPS = $(OBJS:.o=.d)

all: htsim_mainSimulator

htsim_mainSimulator: $(OBJS) ../libhtsim.a
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -lhtsim -o htsim_mainSimulator

%.o: %.cpp
	$(CC) $(INCLUDE) $(CFLAGS) $(DEPFLAGS) -c $<

clean:	
	rm -f *.o *.d htsim*

-include $(DEPS)
