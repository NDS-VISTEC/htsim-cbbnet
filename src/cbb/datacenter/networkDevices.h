#ifndef NETDEV
#define NETDEV
#include <vector>
#include "eventlist.h"
#include "config.h"
#include "tor.h"

class Queue;
class Pipe;
class RlbModule;
class dynamicTopology;
class ToR;
class ControllerSink;
class Controller;
class networkDevices {
    //friend class dynamicTopology;
    public:
    
        networkDevices(mem_b queuesize, dynamicTopology *top, EventList *eventlist, simtime_picosec delay, uint64_t link_rate, double pdrop);

        Pipe* get_pipe_serv_tor(int node) {return pipes_serv_tor[node];}
        Queue* get_queue_serv_tor(int node) {return queues_serv_tor[node];}
        Pipe* get_pipe_tor(int tor, int port) {return pipes_tor[tor][port];}
        Queue* get_queue_tor(int tor, int port); 
        RlbModule* get_rlb_module(int host) {return rlb_modules[host];}
        ToR* get_top_of_rack(int tor) {return top_of_rack_switch[tor];}
        dynamicTopology* _top;
        simtime_picosec link_delay;
        uint64_t get_link_rate() {return _link_rate;}
        void init_controller_sink(Controller* con);
        ControllerSink* _conSink;

    private:
    
        vector<Pipe*> pipes_serv_tor; // vector of pointers to pipe
        vector<Queue*> queues_serv_tor; // vector of pointers to queue
        vector<RlbModule*> rlb_modules; // each host has an rlb module
        vector<vector<Pipe*>> pipes_tor; // matrix of pointers to pipe
        vector<vector<Queue*>> queues_tor; // matrix of pointers to queue
        vector<ToR*> top_of_rack_switch;
        uint64_t _link_rate;
        double _pdrop;

};



#endif