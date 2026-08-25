CC = g++
CFLAGS = -Wall -g -O3
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
#-I$(CRT)/ksp -I$(CRT)/ksp/boost
LIB=-L.. 
#-Lksp

all:	htsim_ndp_fatTree_3to1_k12 


htsim_ndp_fatTree_3to1_k12: main_ndp_fatTree_3to1_k12.o firstfit.o ../libhtsim.a fat_tree_topology_3to1_k12.o
	$(CC) $(CFLAGS) firstfit.o main_ndp_fatTree_3to1_k12.o fat_tree_topology_3to1_k12.o $(LIB) -lhtsim -o htsim_ndp_fatTree_3to1_k12



main_ndp_fatTree_3to1_k12.o: main_ndp_fatTree_3to1_k12.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_fatTree_3to1_k12.cpp 


firstfit.o: firstfit.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c firstfit.cpp

fat_tree_topology_3to1_k12.o: fat_tree_topology_3to1_k12.cpp fat_tree_topology_3to1_k12.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_topology_3to1_k12.cpp


clean:	
	rm -f *.o htsim_ndp* htsim_tcp* htsim_dctcp*
