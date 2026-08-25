#include "controller.h"
#include "cbb_logging.h"
#include "pipe.h"

PacketDB<ControlPacket> ControlPacket::_packetdb;

Controller::Controller(networkDevices* net, dynamicTopology* top, EventList& eventlist ,int host, int interval, int control_pkt_slice): EventSource(eventlist, "controller"){
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "init Controller" << endl;
    _host = host;
    _interval = interval;
    _control_pkt_slice = control_pkt_slice;
    _top = top;
    _super_slice_time = _top->get_slicetime(3);
    _net = net;
    int nToRs = top->no_of_tors();
    demand_matrix = vector<vector<int>>(nToRs, vector<int>(nToRs, 0));
}

void Controller::init(){
    simtime_picosec next_event_time = _super_slice_time*(_interval - _control_pkt_slice);
    eventlist().sourceIsPendingRel(*this, next_event_time);
}

void Controller::doNextEvent(){
    collect_demand();
    simtime_picosec next_time = _super_slice_time*_interval;
    eventlist().sourceIsPendingRel(*this, next_time);
}

void Controller::collect_demand(){
    int nToR = _top->no_of_tors();
    for (int t = 0; t < nToR; t++){
        _net->get_top_of_rack(t)->collect_demand();
    }
}



void Controller::broadcast_update(){
    // create the first pkt to send to src ToR
    ControlPacket* p = ControlPacket::newpkt(_net, _top, NULL, NULL, 0, 0, _broadcast_pkt_size);
    p->set_crtToR(-1); // send from host to ToR set to -1, it will auto change in pipe
    p->set_crthop(0);
    p->to_controller(false);
    p->set_lasthop(false);
    Pipe* Pipe_serve_ToR = _net->get_pipe_serv_tor(0); // set central controller at host 0
    Pipe_serve_ToR->receivePacket(*p);

}



ControllerSink::ControllerSink(Controller* cnt, networkDevices* net, double pdrop){
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "init Controller Sink" << endl;
    _cnt = cnt;
    _net = net;

    int seed = 1000;
    _rng = mt19937(seed);
    _dist = uniform_real_distribution<double>(0.0, 1.0);
    _pdrop = pdrop;
}


void ControllerSink::receivePacket(Packet& pkt){
    double rand_num = _dist(_rng);

    if (rand_num < _pdrop){
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "drop control pkt from " << pkt.get_src_ToR() << endl;
        pkt.free();
    }
    else{
        int srcToR = pkt.get_src_ToR();
        // cout << "pkt size:" << pkt.size() << " " << srcToR << endl;
        if(pkt.size() > CONTROL_HEADER + 1){
            vector<bool> demand = _net->get_top_of_rack(srcToR)->get_local_demand();
            for (int dstToR = 0; dstToR < (int)demand.size(); dstToR++){
                if (demand[dstToR]){
                    // cout << "src: " << srcToR << " dst: " << dstToR << endl;
                    _cnt->demand_matrix[srcToR][dstToR] = 1;
                    // _cnt->demand_matrix[dstToR][srcToR] = 1;
                }
            }
        }
        else{
            for (int dstToR = 0; dstToR < (int)_cnt->demand_matrix.size(); dstToR++){
                _cnt->demand_matrix[srcToR][dstToR] = 0;
            }
        }

        pkt.free();
    }

}
