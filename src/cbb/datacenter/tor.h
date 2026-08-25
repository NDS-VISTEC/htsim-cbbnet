#ifndef TOR_H
#define TOR_H
#include "network.h"

class networkDevices;
class dynamicTopology;
class RlbModule;
class Queue;
class ControllerSink;
class ToR{
    public:
        ToR(networkDevices* net, dynamicTopology* top,int tor_id, int ntor, int nul, int ndl);
        int get_output_port(Packet* pkt, int slice);
    public:
        void set_queue(int port, Queue* queue);
        Queue* get_queue(int port) const;
        void set_local_rlb_module(int host, RlbModule* module);
        void set_controllerSink(ControllerSink* consink){ _conSink = consink; }
        void collect_demand();
        void send_control_pkt(bool has_demand);
        vector<bool> get_local_demand(){return _demands;}

        void receivePacket(Packet &pkt);
        bool receiveNewMapping(){ return _receiveNewMapping; }
        int get_tor_id(){return _tor_id;}
    private:
        int _tor_id;
        int _nul;
        int _ndl;
        int _ntor;
        bool _receiveNewMapping;
        dynamicTopology* _top;
        networkDevices* _net;
        vector<int> _path_index;
        vector<bool> _demands;
        ControllerSink* _conSink;
        vector<Queue*> queues_tor;
        vector<RlbModule*> local_rlb_moduls;

        int _demand_pkt_size = 16; //bytes (for 128 ToRs)
        int _mapping_plt_size = 256; //bytes (for 128 ToRs)
        vector<vector<vector<int>>> _childen_ToRs; // use for fowarding mapping packet

        vector<bool> _used_ports;
};


#endif
