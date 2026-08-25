// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "pipe.h"
#include <iostream>
#include <sstream>

#include "queue.h"
#include "ndp.h"
#include "tcp.h"
#include "rlbmodule.h"
#include "ndppacket.h"
#include "rlbpacket.h" // added for debugging
#include "tcppacket.h"
#include "controller.h"
#include "cbb_logging.h"


Pipe::Pipe(simtime_picosec delay, EventList& eventlist)
: EventSource(eventlist,"pipe"), _delay(delay)
{
    //stringstream ss;
    //ss << "pipe(" << delay/1000000 << "us)";
    //_nodename= ss.str();

    _bytes_delivered = 0;
    _bytes_delivered_total = 0;

}

void Pipe::receivePacket(Packet& pkt)
{
    if (_inflight.empty()){
        /* no packets currently inflight; need to notify the eventlist
            we've an event pending */
        eventlist().sourceIsPendingRel(*this,_delay);
    }
    _inflight.push_front(make_pair(eventlist().now() + _delay, &pkt));
}

void Pipe::doNextEvent() {
    if (_inflight.size() == 0) 
        return;
    
    Packet *pkt = _inflight.back().second;
    _inflight.pop_back();
    
    // tell the packet to move itself on to the next hop
    sendFromPipe(pkt);

    if (!_inflight.empty()) {
        // notify the eventlist we've another event pending
        simtime_picosec nexteventtime = _inflight.back().first;
        //cout << "Pipe pending next: " << nexteventtime << endl;
        _eventlist.sourceIsPending(*this, nexteventtime);
    }

}

uint64_t Pipe::reportBytes() {
    uint64_t temp;
    temp = _bytes_delivered;
    _bytes_delivered = 0; // reset the counter
    return temp;
}

uint64_t Pipe::reportBytesTotal() {
    uint64_t temp;
    temp = _bytes_delivered_total;
    _bytes_delivered_total = 0; // reset the counter
    return temp;
}

void Pipe::sendFromPipe(Packet *pkt) {

        _bytes_delivered_total = _bytes_delivered_total + pkt->size(); 

        if (pkt->is_lasthop()) {
            switch (pkt->type()) {
            case RLB:
            {
                
                // check if it's really the last hop for this packet
                // otherwise, it's getting indirected, and doesn't count.
                if (pkt->get_dst() == pkt->get_real_dst()) {

                    _bytes_delivered = _bytes_delivered + pkt->size(); // increment packet delivered

                }
                networkDevices* net = pkt->get_device();
                RlbModule* mod = net->get_rlb_module(pkt->get_dst());
                
                assert(mod);
                mod->receivePacket(*pkt, 0);
                break;
            }
            case NDP:
            {
                if (pkt->bounced() == false) {

                    //NdpPacket* ndp_pkt = dynamic_cast<NdpPacket*>(pkt);
                    //if (!ndp_pkt->retransmitted())
                    if (pkt->size() > 64) // not a header
                        _bytes_delivered = _bytes_delivered + pkt->size(); // increment packet delivered

                    // send it to the sink
                    NdpSink* sink = pkt->get_ndpsink();
                    assert(sink);


                    sink->receivePacket(*pkt);
                } else {

               

                    // send it to the source
                    NdpSrc* src = pkt->get_ndpsrc();
                    assert(src);
                    src->receivePacket(*pkt);
                }
                
                break;
            }
            case NDPACK:
            case NDPNACK:
            case NDPPULL:
            {
                NdpSrc* src = pkt->get_ndpsrc();
                assert(src);
                src->receivePacket(*pkt);
                break;
            }
            case TCP:
            {
                //cout << "Go to TCP\n";
                TcpSink* sink = pkt->get_tcpsink();
                assert(sink);
                sink->receivePacket(*pkt);
                break;
            }
            case TCPACK:
            {
                TcpSrc* src = pkt->get_tcpsrc();
                assert(src);
                src->receivePacket(*pkt);
                break;
            }
            case CBB_CNT:
            {
                ControllerSink* conSink = pkt->get_conSink();
                assert(conSink);
                conSink->receivePacket(*pkt);

                break;
            }
            }
        } else {
            // we'll be delivering to a ToR queue
            dynamicTopology* top = pkt->get_topology();
            networkDevices* net = pkt->get_device();

            // send from nic
            if (pkt->get_crtToR() < 0) {
                pkt->set_crtToR(pkt->get_src_ToR());
            }
            else {
                int64_t superslice = (eventlist().now() / top->get_slicetime(3)) %
                    top->get_nsuperslice();
                // next, get the relative time from the beginning of that superslice
                simtime_picosec reltime = eventlist().now() - superslice*top->get_slicetime(3) -
                    (eventlist().now() / (top->get_nsuperslice()*top->get_slicetime(3))) * 
                    (top->get_nsuperslice()*top->get_slicetime(3));
                int slice; // the current slice
                if (reltime < top->get_slicetime(0))
                    slice = 0 + superslice*3;
                else if (reltime < top->get_slicetime(0) + top->get_slicetime(1))
                    slice = 1 + superslice*3;
                else
                    slice = 2 + superslice*3;
                int nextToR = top->get_nextToR(slice, pkt->get_crtToR(), pkt->get_crtport());
			    if (nextToR >= 0) {
                    pkt->set_crtToR(nextToR);
			    }
                else {
                    // cout << "pkt" << pkt->get_slice_sent() << " " << top->getSuperSlice()*3 << endl;
                    vector<int> path = top->get_lbls_path(pkt->get_src()/top->no_of_hpr(), pkt->get_dst()/top->no_of_hpr(), pkt->get_slice_sent());
                    switch (pkt->type()) {
                    case RLB:
                    {
                        // for now, let's just return the packet rather than implementing the RLB NACK mechanism
                        
                        RlbPacket *p = (RlbPacket*)(pkt);
                        // cout << "Pipe: Going down to " << p->get_src() << endl;
                        //RlbModule* module = top->get_rlb_module(p->get_src()); // returns pointer to Rlb module that sent the packet
                        RlbModule *module = net->get_rlb_module(p->get_src());
                        module->receivePacket(*p, 1); // 1 means to put it at the front of the queue
                        break;
                    }
                    case TCP:
                        cbb_logging::error() << "!!! TCP packet clipped in pipe (rotor switch down)" << endl;
                        cbb_logging::error() << "    time = " << timeAsUs(eventlist().now()) << " us";
                        //cout << "    current slice = " << slice << endl;
                        cbb_logging::error() << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cbb_logging::error() << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case TCPACK:
                        cbb_logging::error() << "!!! TCP ACK clipped in pipe (rotor switch down)" << endl;
                        cbb_logging::error() << "    time = " << timeAsUs(eventlist().now()) << " us";
                        //cout << "    current slice = " << slice << endl;
                        cbb_logging::error() << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cbb_logging::error() << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case NDP:
                        cbb_logging::error() << "!!! NDP packet clipped in pipe (rotor switch down)" << endl;
                        cbb_logging::error() << "    time = " << timeAsUs(eventlist().now()) << " us";
                        //cout << "    current slice = " << slice << endl;
                        cbb_logging::error() << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cbb_logging::error() << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        for (auto x: path) {cbb_logging::error() << x << " ";}
                        cbb_logging::error() << endl;
                        pkt->free();
                        break;
                    case NDPACK:
                        cbb_logging::error() << "!!! NDP ACK clipped in pipe (rotor switch down)" << endl;
                        cbb_logging::error() << "    time = " << timeAsUs(eventlist().now()) << " us";
                        //cout << "    current slice = " << slice << endl;
                        cbb_logging::error() << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cbb_logging::error() << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        for (auto x: path) {cbb_logging::error() << x << " ";}
                        cbb_logging::error() << endl;
                        pkt->free();
                        break;
                    case NDPNACK:
                        cbb_logging::error() << "!!! NDP NACK clipped in pipe (rotor switch down)" << endl;
                        cbb_logging::error() << "    time = " << timeAsUs(eventlist().now()) << " us";
                        //cout << "    current slice = " << slice << endl;
                        cbb_logging::error() << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cbb_logging::error() << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        for (auto x: path) {cbb_logging::error() << x << " ";}
                        cbb_logging::error() << endl;
                        pkt->free();
                        break;
                    case NDPPULL:
                        cbb_logging::error() << "!!! NDP PULL clipped in pipe (rotor switch down)" << endl;
                        cbb_logging::error() << "    time = " << timeAsUs(eventlist().now()) << " us";
                        //cout << "    current slice = " << slice << endl;
                        cbb_logging::error() << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cbb_logging::error() << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        for (auto x: path) {cbb_logging::error() << x << " ";}
                        cbb_logging::error() << endl;
                        pkt->free();
                        break;
                    }
                    return;
                }

            }

            pkt->inc_crthop(); // increment the hop
            int next_tor = pkt->get_crtToR();
            ToR* top_of_rack = net->get_top_of_rack(next_tor);

            top_of_rack->receivePacket(*pkt);

        }
    //}
    // cout << "end func sendFromPipe..." << endl;
}

//////////////////////////////////////////////
//      Aggregate utilization monitor       //
//////////////////////////////////////////////


UtilMonitor::UtilMonitor(dynamicTopology* top, networkDevices* net, EventList &eventlist)
  : EventSource(eventlist,"utilmonitor"), _top(top), _net(net)
{
    _H = _top->no_of_nodes(); // number of hosts
    _N = _top->no_of_tors(); // number of racks
    _hpr = _top->no_of_hpr(); // number of hosts per rack
    // uint64_t rate = 10000000000 / 8; // bytes / second
    uint64_t rate = net->get_link_rate(); // bytes / second
    rate = rate * _H;
    //rate = rate / 1500; // total packets per second

    _max_agg_Bps = rate;

    // debug:
    //cout << "max bytes per second = " << rate << endl;

}

void UtilMonitor::start(simtime_picosec period) {
    _period = period;
    _max_B_in_period = _max_agg_Bps * timeAsSec(_period);

    // debug:
    //cout << "_max_pkts_in_period = " << _max_pkts_in_period << endl;

    eventlist().sourceIsPending(*this, _period);
}

void UtilMonitor::doNextEvent() {
    printAggUtil();
}

void UtilMonitor::printAggUtil() {

    uint64_t B_sum = 0;

    for (int tor = 0; tor < _N; tor++) {
        for (int downlink = 0; downlink < _hpr; downlink++) {
            Pipe* pipe = _net->get_pipe_tor(tor, downlink);
            B_sum = B_sum + pipe->reportBytesTotal();
        }
    }

    // debug:
    //cout << "Packets counted = " << (int)pkt_sum << endl;
    //cout << "Max packets = " << _max_pkts_in_period << endl;

    double util = (double)B_sum / (double)_max_B_in_period;

    cout << "Util " << fixed << util << " " << timeAsMs(eventlist().now()) << endl;
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Num event: " << eventlist().get_event_size() << endl;

    uint64_t B_sum_up = 0;

    for (int hst = 0; hst < _H; hst++) {
        Pipe* pipe = _net->get_pipe_serv_tor(hst);
        B_sum_up = B_sum_up + pipe->reportBytesTotal();
    }

    double util_up = (double)B_sum_up / (double)_max_B_in_period;

    cout << "Util_up " << fixed << util_up << " " << timeAsMs(eventlist().now()) << endl;
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Num event: " << eventlist().get_event_size() << endl;

    if (eventlist().now() + _period < eventlist().getEndtime())
        eventlist().sourceIsPendingRel(*this, _period);

}
