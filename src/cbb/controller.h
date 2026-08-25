#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "network.h"
#include "eventlist.h"
#include "dynamicTopology.h"
#include "networkDevices.h"
#include <random>
#define CONTROL_HEADER 64


class Controller : public EventSource{
    public:
        Controller(networkDevices* net, dynamicTopology* top, EventList& eventlist, int host, int interval, int control_pkt_slice);
        void init();
        void doNextEvent();
        void collect_demand();
        vector<vector<int>> demand_matrix;

        void broadcast_update();
        
    private:
        int _host;
        int _interval;
        int _control_pkt_slice;
        dynamicTopology* _top;
        networkDevices* _net;
        simtime_picosec _super_slice_time;
        int _broadcast_pkt_size = 256;

};


class ControllerSink{
    public:
        ControllerSink(Controller* cnt, networkDevices* net, double pdrop);
        void receivePacket(Packet& pkt);
    private:
        Controller* _cnt;
        networkDevices* _net;
        mt19937 _rng;
        uniform_real_distribution<double> _dist;
        float _pdrop;
};



class ControlPacket : public Packet{
    public:

        inline static ControlPacket* newpkt(networkDevices* net, dynamicTopology* top, ControllerSink* conSink, PacketFlow flow, int srcToR, int dst, int size){
            ControlPacket* p = _packetdb.allocPacket();
            p->set_attrs(flow, size+CONTROL_HEADER, 0, srcToR, dst);
            p->set_topology(top);
            p->set_device(net);
            p->_type = CBB_CNT;
            p->_is_header = false;
	        p->_bounced = false;
            p->_conSink = conSink;
            p->set_src_ToR(srcToR);
            return p;
        }


        virtual inline void  strip_payload() {
	        Packet::strip_payload(); _size = CONTROL_HEADER;
        };

        void free() { _packetdb.freePacket(this); }
        virtual ~ControlPacket(){}
        virtual inline ControllerSink* get_conSink(){ return _conSink; }
    protected:
        static PacketDB<ControlPacket> _packetdb;
        ControllerSink* _conSink;
};

#endif