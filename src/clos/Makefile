CRT= `pwd`
INC= -I$(CRT)/datacenter -I$(CRT)

CC=g++
CFLAGS = -Wall -g -O3 $(INC)

OBJS=eventlist.o tcppacket.o pipe.o queue.o queue_lossless.o queue_lossless_input.o queue_lossless_output.o ecnqueue.o tcp.o dctcp.o mtcp.o loggers.o logfile.o clock.o config.o network.o qcn.o exoqueue.o randomqueue.o cbr.o cbrpacket.o sent_packets.o ndp.o ndppacket.o eth_pause_packet.o compositequeue.o prioqueue.o cpqueue.o compositeprioqueue.o switch.o fairpullqueue.o route.o

all:	lib 

lib:	$(OBJS) 
	ar -rcs libhtsim.a $(OBJS)

clean:	
	rm -f *.o htsim htsim_* libhtsim.a

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@
