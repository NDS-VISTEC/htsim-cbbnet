// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include <sstream>
#include <math.h>
#include "queue.h"
#include "ndppacket.h"
#include "rlbpacket.h" // added
#include "queue_lossless.h"

#include "pipe.h"

#include "rlb.h" // needed to make dummy packet
#include "rlbmodule.h"
#include "cbb_logging.h"

Queue::Queue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist)
  : EventSource(eventlist,"queue"), _maxsize(maxsize), _bitrate(bitrate), _num_drops(0)
{
    _queuesize = 0;
    _ps_per_byte = (simtime_picosec)((pow(10.0, 12.0) * 8) / _bitrate);
    stringstream ss;
    //ss << "queue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
    //_nodename = ss.str();
}


void Queue::beginService() {
    /* schedule the next dequeue event */
    assert(!_enqueued.empty());
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued.back()));
}

void Queue::completeService() {
    /* dequeue the packet */
    assert(!_enqueued.empty());
    Packet* pkt = _enqueued.back();
    _enqueued.pop_back();
    _queuesize -= pkt->size();

    /* tell the packet to move on to the next pipe */
    //pkt->sendFromQueue();
    sendFromQueue(pkt);

    if (!_enqueued.empty()) {
        /* schedule the next dequeue event */
        beginService();
    }
}

void Queue::sendFromQueue(Packet* pkt) {

    // cout << "begin send from Q at" << eventlist().now() << endl;
    Pipe* nextpipe; // the next packet sink will be a pipe
    dynamicTopology* top = pkt->get_topology();
    networkDevices* net = pkt->get_device();
        if (pkt->get_crthop() < 0) {

            nextpipe = net->get_pipe_serv_tor(pkt->get_src());
            nextpipe->receivePacket(*pkt);

        } else {

            // cout << "sending out of ToR" << endl;

            // we're sending out of a ToR queue
            if (top->is_last_hop(pkt->get_crtport())) {
                pkt->set_lasthop(true);
                // if this port is not connected to _dst, then drop the packet
                if (!top->port_dst_match(pkt->get_crtport(), pkt->get_crtToR(), pkt->get_dst())) {

                    switch (pkt->type()) {
                    case RLB:
                        cbb_logging::error() << "!!! RLB";
                        break;
                    case NDP:
                        cbb_logging::error() << "!!! NDP";
                        break;
                    case NDPACK:
                        cbb_logging::error() << "!!! NDPACK";
                        break;
                    case NDPNACK:
                        cbb_logging::error() << "!!! NDPNACK";
                        break;
                    case NDPPULL:
                        cbb_logging::error() << "!!! NDPPULL";
                        break;
                    case CBB_CNT:
                        cbb_logging::error() << "!!! CBB_CNT";
                        break;
                    }
                    cbb_logging::error() << " packet dropped: port & dst didn't match! (queue.cpp)" << endl;
                    cbb_logging::error() << "    ToR = " << pkt->get_crtToR() << ", port = " << pkt->get_crtport() <<
                        ", src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << " at " << eventlist().now() << endl;

                    cbb_logging::error() << "real_src = " << pkt->get_real_src() << ", real_dst = " << pkt->get_real_dst() << endl;
                    pkt->free(); // drop the packet
                    
                    return;
                }
            }
            nextpipe = net->get_pipe_tor(pkt->get_crtToR(), pkt->get_crtport());
            nextpipe->receivePacket(*pkt);
        }
}

void Queue::doNextEvent() {
    // cout <<< "Queue doing next event at " << timeAsUs(eventlist().now()) <<endl;
    completeService();

}


void Queue::receivePacket(Packet& pkt) {
    // cout << "Ald inside Queue::receivePacket..." << endl;
    if (_queuesize+pkt.size() > _maxsize) {
        /* if the packet doesn't fit in the queue, drop it */
        //if (_logger) 
        //    _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
        pkt.free();
        _num_drops++;
        return;
    }

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);

    /* enqueue the packet */
    bool queueWasEmpty = _enqueued.empty();
    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();
    if (queueWasEmpty) {
	assert(_enqueued.size() == 1);
    beginService();
    }
}

mem_b Queue::queuesize() {
    return _queuesize;
}

simtime_picosec Queue::serviceTime() {
    return _queuesize * _ps_per_byte;
}

//////////////////////////////////////////////////
//              Priority Queue                  //
//////////////////////////////////////////////////

PriorityQueue::PriorityQueue(networkDevices* net, dynamicTopology* top, linkspeed_bps bitrate, mem_b maxsize, 
			     EventList& eventlist, int node)
    : Queue(bitrate, maxsize, eventlist)
{
    _net = net;
    _top = top;
    _node = node;

    _bytes_sent = 0;
    directCount = 0;

    _queuesize[Q_RLB] = 0;
    _queuesize[Q_LO] = 0;
    _queuesize[Q_MID] = 0;
    _queuesize[Q_HI] = 0;
    _servicing = Q_NONE;
    //_state_send = LosslessQueue::READY;
}

PriorityQueue::queue_priority_t PriorityQueue::getPriority(Packet& pkt) {
    queue_priority_t prio = Q_LO;
    switch (pkt.type()) {
    case TCPACK:
        prio = Q_HI;
        break;
    case NDPACK:
    case NDPNACK:
    case NDPPULL:
    case NDPLITEACK:
    case NDPLITERTS:
    case NDPLITEPULL:
        prio = Q_HI;
        break;
    case NDP:
        if (pkt.header_only()) {
            prio = Q_HI;
        } else {
            NdpPacket* np = (NdpPacket*)(&pkt);
            if (np->retransmitted()) {
                prio = Q_MID;
            } else {
                prio = Q_LO;
            }
        }
        break;
    case RLB:
        prio = Q_RLB;
        break;
    case TCP:
        prio = Q_LO;
        break;
    case IP:
    case NDPLITE:
        prio = Q_LO;
        break;
    default:
                cbb_logging::error() << "NIC couldn't identify packet type." << endl;
        abort();
    }
    //cout << prio << endl;
    return prio;
}

simtime_picosec PriorityQueue::serviceTime(Packet& pkt) {
    queue_priority_t prio = getPriority(pkt);
    switch (prio) {
    case Q_LO:
	   //cout << "q_lo: " << _queuesize[Q_HI] + _queuesize[Q_MID] + _queuesize[Q_LO] << " ";
	   return (_queuesize[Q_HI] + _queuesize[Q_MID] + _queuesize[Q_LO]) * _ps_per_byte;
    case Q_MID:
	   //cout << "q_mid: " << _queuesize[Q_MID] + _queuesize[Q_LO] << " ";
	   return (_queuesize[Q_HI] + _queuesize[Q_MID]) * _ps_per_byte;
    case Q_HI:
	   //cout << "q_hi: " << _queuesize[Q_LO] << " ";
	   return _queuesize[Q_HI] * _ps_per_byte;
    case Q_RLB:
        abort(); // we should never check this for an RLB packet
    default:
	   abort();
    }
}

void PriorityQueue::doorbell(bool rlbwaiting) {

    if (rlbwaiting) { // add a dummy packet to the queue

        RlbPacket* pkt = RlbPacket::newpkt(1500); // make a dummy packet
        pkt->set_dummy(true);
        receivePacket(*pkt); // put that dummy packet in the RLB queue
            // ! note - use `receivePacket` so we trigger service to begin
    } else {
        // the RLB module isn't ready to send more packets right now
        // drop the dummy packet in the queue so we don't try to pull from the RLB module

        Packet* pkt = _queue[0].front(); // RLB enumerates to 0
        pkt->free();
        _queue[0].pop_front();
        _queuesize[0] = 0; // set queuesize to zero. 
    }
}

void PriorityQueue::receivePacket(Packet& pkt) {

    queue_priority_t prio = getPriority(pkt);
    /* enqueue the packet */
    bool queueWasEmpty = false;
    if (queuesize() == 0)
        queueWasEmpty = true;

    _queuesize[prio] += pkt.size();
    _queue[prio].push_front(&pkt);


    if (queueWasEmpty) {
        /* schedule the dequeue event */
        assert(_queue[Q_RLB].size() + _queue[Q_LO].size() + _queue[Q_MID].size() + _queue[Q_HI].size() == 1);
        beginService();
    }
}

void PriorityQueue::beginService() {

    /* schedule the next dequeue event */
    for (int prio = Q_HI; prio >= Q_RLB; --prio) {
        if (_queuesize[prio] > 0) {  
            // eventlist().sourceIsPendingRel(*this, drainTime(_queue[prio].back()));
            if (prio > 0) {
                // cout << "prio Q pending at " << eventlist().now() << " will do at "<< eventlist().now() + drainTime(_queue[prio].back())*_top->no_of_upl()/_top->no_of_hpr() << endl;
                eventlist().sourceIsPendingRel(*this, drainTime(_queue[prio].back())*_top->no_of_upl()/_top->no_of_hpr());
            }
            else {
                eventlist().sourceIsPendingRel(*this, drainTime(_queue[prio].back())); //xxxx
            }
           
            _servicing = (queue_priority_t)prio;

            return;
        }
    }
}

void PriorityQueue::completeService() {


    // debug:
    //if (_node == 0)
    //    cout << " NIC: pulling a packet" << endl;

    // debug:
    //cout << "NIC[node" << _node << "] - completeService() at " << timeAsUs(eventlist().now()) << " us." << endl;

	// debug:
    //if (_node == 345 && timeAsUs(eventlist().now()) > 5044) {
    //    cout << "   completeService on _servicing: [" << _servicing << "] at " << timeAsUs(eventlist().now()) << " us." << endl;
    //}
    
	// debug:
	//if (_queue[_servicing].empty()) {
	//	cout << "_node = " << _node << endl;
	//	cout << "servicing: " << _servicing << endl;
	//	cout << "time = " << timeAsUs(eventlist().now()) << endl;
	//}

	// the below doesn't work because RLB can shut down the queue in between `beginService` and `completeService`
    //assert(!_queue[_servicing].empty());
    //assert(_servicing != Q_NONE);
    //cout << _queue[Q_RLB].size() << endl;

	if (!_queue[_servicing].empty()) {

	    Packet* pkt;

	    switch (_servicing) {
	    case Q_RLB:
	    {
                RlbModule* mod = _net->get_rlb_module(_node);
                int _have_packets = mod->get_have_packets();

                if (_have_packets) {
                    pkt = mod->NICpull(); // get the packet from the RLB module

                    // check if the packet is a dummy (spacer packet for rate limiting)
                    // If so, free the packet, and return;
                    if (pkt->is_dummy()) {
                        pkt->free();

                        // debug
                        //if (_node == 0)
                        //cout << "  dummy packet; drop." << endl;

                        break;
                    } else {

                        pkt->set_src_ToR(_top->get_firstToR(pkt->get_src())); // set the sending ToR. This is used for subsequent routing

                        // send on the first path (index 0) to the "intermediate" destination
                        int path_index = 0; // index 0 ensures it's the direct path
                        pkt->set_path_index(path_index); // set which path the packet will take

                        // set some initial packet parameters used for routing
                        pkt->set_lasthop(false);
                        pkt->set_crthop(-1);
                        pkt->set_crtToR(-1);

                        pkt->set_maxhops(_top->get_no_hops(pkt->get_src_ToR(),
                            _top->get_firstToR(pkt->get_dst()), pkt->get_slice_sent(), path_index));
                        
                    }

                    sendFromQueue(pkt);
                }
                    

            //}

	        break;
	    }
	    case Q_LO:
	    case Q_MID:
	    case Q_HI:
	    {

	        pkt = _queue[_servicing].back(); // get the pointer to the packet
	        _queue[_servicing].pop_back(); // delete the element of the queue
	        _queuesize[_servicing] -= pkt->size(); // decrement the queue size

	        int new_bytes_sent = _bytes_sent + pkt->size();
	        if (new_bytes_sent / 1500 > _bytes_sent / 1500) {
	            // we sent a "full" packet ahead of RLB, notify RLB to push
	            //RlbModule* mod = _top->get_rlb_module(_node);
                //cout << "NICpush: with new_byte_sent and byte_sent = " << new_bytes_sent << " " << _bytes_sent << endl;
                RlbModule* mod = _net->get_rlb_module(_node);
	            mod->NICpush();
	        }
	        _bytes_sent = new_bytes_sent;



	        // set the routing info
            //cout << "HI Priority\n";

            pkt->set_src_ToR(_top->get_firstToR(pkt->get_src())); // set the sending ToR. This is used for subsequent routing

	        if (pkt->get_src_ToR() == _top->get_firstToR(pkt->get_dst())) {
	            // the packet is being sent within the same rack
	            pkt->set_lasthop(false);
	            pkt->set_crthop(-1);
	            pkt->set_crtToR(-1);
	            pkt->set_maxhops(0); // want to select a downlink port immediately
	        } else {
	            // the packet is being sent between racks

                int64_t superslice = _top -> getSuperSlice();
                int npaths = 1;

                int64_t reltime = calculateRelTime(eventlist().now(), _top->get_slicetime(3));
                int slice; // the current slice
	            if (reltime < (int64_t)_top->get_slicetime(0))
	                slice = 0 + superslice*3;
	            else if (reltime < (int64_t)_top->get_slicetime(0) + (int64_t)_top->get_slicetime(1))
	                slice = 1 + superslice*3;
	            else
	                slice = 2 + superslice*3;

	            if (npaths == 0)
                cbb_logging::error() << "Error: there were no paths!" << endl;
	            assert(npaths > 0);

	            // randomly choose a path for the packet
	            // !!! todo: add other options like permutation, etc...
	            // int path_index = rand() % npaths; //
	            pkt->set_slice_sent(slice); // "timestamp" the packet
	            // pkt->set_path_index(path_index); // set which path the packet will take //

                int path_index = pkt->get_path_index(); //
	            // set some initial packet parameters used for label switching
	            // *this could also be done in NDP before the packet is sent to the NIC
	            pkt->set_lasthop(false);
	            pkt->set_crthop(-1);
	            pkt->set_crtToR(-1);
	            pkt->set_maxhops(_top->get_no_hops(pkt->get_src_ToR(),
	                _top->get_firstToR(pkt->get_dst()), slice, path_index));
	        }
            //cout << "Send to next pipe\n";
	        /* tell the packet to move on to the next pipe */
            sendFromQueue(pkt);

	        break;
	    }
	    case Q_NONE:
	    	break;
	        //abort();
	    }
	}

    if (queuesize() > 0) {
        // cout << "NIC beginService at " << eventlist().now() << endl;
        beginService();
    } else {

        // debug:
        //cout << "NIC stopped sending" << endl;

        _servicing = Q_NONE;
    }
}

mem_b PriorityQueue::queuesize() {
    return _queuesize[Q_RLB] + _queuesize[Q_LO] + _queuesize[Q_MID] + _queuesize[Q_HI];
}
