CC = g++
CFLAGS = -Wall -g -O3 $(INC)
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
LIB=-L.. 

OBJS = main_ndp_dynexpTopology.o dynexp_topology.o

all:	htsim_ndp_dynexpTopology


htsim_ndp_dynexpTopology: $(OBJS) ../libhtsim.a 
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -lhtsim -o htsim_ndp_dynexpTopology

%.o: %.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c $<

clean:	
	rm -f *.o htsim*
