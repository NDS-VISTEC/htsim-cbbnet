// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-    
#include <math.h>
#include <iostream>
#include "rlb.h"
#include "cbb_logging.h"
#include "queue.h"
#include <stdio.h>

#include "rlbmodule.h"

#include "pipe.h"
#include <string>

////////////////////////////////////////////////////////////////
//  RLB SOURCE
////////////////////////////////////////////////////////////////
RlbSrc::RlbSrc(networkDevices* net, dynamicTopology* top, NdpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, int flow_src, int flow_dst, int flowDA)
    : EventSource(eventlist,"rlbsrc"), _flow_src(flow_src), _flow_dst(flow_dst), _top(top), _net(net), _logger(logger), _flow(pktlogger) {
    
    _mss = 1436; // Packet::data_packet_size(); // maximum segment size (mss)
    _sink = 0;
    _pkts_sent = 0;
}

void RlbSrc::connect(RlbSink& sink, simtime_picosec starttime) {
    
    _sink = &sink;
    _flow.id = id; // identify the packet flow with the source that generated it
    _flow._name = _name;
    _sink->connect(*this);

    set_start_time(starttime); // record the start time in _start_time
    eventlist().sourceIsPending(*this,starttime);
}

void RlbSrc::startflow() {
    _sent = 0;
    while (_sent < _flow_size) {
        sendToRlbModule();
    }
}

void RlbSrc::sendToRlbModule() {
    RlbPacket* p = RlbPacket::newpkt(_net, _top, _flow, _flow_src, _flow_dst, _sink, _mss, _pkts_sent);
    // ^^^ this sets the current source and destination (used for routing)
    // RLB module uses the "real source" and "real destination" to make decisions
    p->set_dummy(false);
    p->set_real_dst(_flow_dst); // set the "real" destination
    p->set_real_src(_flow_src); // set the "real" source
    p->set_ts(eventlist().now()); // time sent, not really needed...
    p->set_current_local_src(_flow_src);

    p->set_is_da(0);
    RlbModule* module = _net->get_rlb_module(_flow_src);
    module->receivePacket(*p, 0);
    _sent = _sent + _mss; // increment how many packets we've sent
    _pkts_sent++;
}

void RlbSrc::doNextEvent() {
    startflow();
}


////////////////////////////////////////////////////////////////
//  RLB SINK
////////////////////////////////////////////////////////////////


RlbSink::RlbSink(dynamicTopology* top, networkDevices* net, EventList &eventlist, int flow_src, int flow_dst)
    : EventSource(eventlist,"rlbsnk"), _flow_src(flow_src), _flow_dst(flow_dst), _top(top), _net(net) 
{
    _src = 0;
    _nodename = "rlbsink";
    _total_received = 0;
    _pkts_received = 0;

}

void RlbSink::doNextEvent() {
    // just a hack to get access to eventlist
}

void RlbSink::connect(RlbSrc& src)
{
    _src = &src;
}

// Receive a packet.
void RlbSink::receivePacket(Packet& pkt) {

    RlbPacket *p = (RlbPacket*)(&pkt);
    switch (pkt.type()) {
    case NDP:
    case NDPACK:
    case NDPNACK:
    case NDPPULL:
        cbb_logging::error() << "RLB receiver received an NDP packet!" << endl;
        abort();
    case RLB:
        break;
    }

    _pkts_received++;

    int size = p->size()-HEADER;
    _total_received += size;
    p->free();
    if (_total_received >= _src->get_flowsize()) {
        cout << "FCT " << _src->get_flow_src() << " " << _src->get_flow_dst() << " " << _src->get_flowsize() <<
            " " << timeAsMs(eventlist().now() - _src->get_start_time()) << " " << fixed << timeAsMs(_src->get_start_time()) << " ID " << _src->get_flowid() << endl;
        
    }
}
