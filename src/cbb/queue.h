// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef QUEUE_H
#define QUEUE_H

/*
 * A simple FIFO queue
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"
//#include "networkDevices.h"

//class Packet;

class Queue : public PacketSink, public EventSource {
 public:

    Queue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist);
    virtual void receivePacket(Packet& pkt);
    void doNextEvent();

    void sendFromQueue(Packet* pkt);

    int64_t calculateRelTime(int64_t nowTime, int64_t sliceTime) {
        //assert(nowTime >= sliceTime);

        while (nowTime >= sliceTime) {
            nowTime -= sliceTime;
        }

        return nowTime;
    }

    // should really be private, but loggers want to see
    mem_b _maxsize; 

    inline simtime_picosec drainTime(Packet *pkt) { 
        return (simtime_picosec)(pkt->size() * _ps_per_byte); 
    }
    inline mem_b serviceCapacity(simtime_picosec t) { 
        return (mem_b)(timeAsSec(t) * (double)_bitrate); 
    }
    virtual mem_b queuesize();
    simtime_picosec serviceTime();
    int num_drops() const {return _num_drops;}
    void reset_drops() {_num_drops = 0;}

    virtual void setRemoteEndpoint(Queue* q) {_remoteEndpoint = q;};
    virtual void setRemoteEndpoint2(Queue* q) {_remoteEndpoint = q;q->setRemoteEndpoint(this);};
    Queue* getRemoteEndpoint() {return _remoteEndpoint;}

    virtual void setName(const string& name) {
        Logged::setName(name);
        _nodename += name;
    }
    //virtual void setLogger(QueueLogger* logger) {
    //    _logger = logger;
    //}
    virtual const string& nodename() { return _nodename; }

 protected:
    // Housekeeping
    Queue* _remoteEndpoint;

    //QueueLogger* _logger;

    // Mechanism
    // start serving the item at the head of the queue
    virtual void beginService(); 

    // wrap up serving the item at the head of the queue
    virtual void completeService(); 

    linkspeed_bps _bitrate; 
    simtime_picosec _ps_per_byte;  // service time, in picoseconds per byte
    mem_b _queuesize;
    list<Packet*> _enqueued;
    int _num_drops;
    string _nodename;
};

/* implement a 4-level priority queue */
// modified to include RLB
class PriorityQueue : public Queue {
 public:
    typedef enum {Q_RLB=0, Q_LO=1, Q_MID=2, Q_HI=3, Q_NONE=4} queue_priority_t;
    PriorityQueue(networkDevices* net, dynamicTopology* top, linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, int node);
    virtual void receivePacket(Packet& pkt);
    virtual mem_b queuesize();
    simtime_picosec serviceTime(Packet& pkt);

    void doorbell(bool rlbwaiting);

    int _bytes_sent; // keep track so we know when to send a push to RLB module

    dynamicTopology* _top;
    networkDevices* _net;
    int _node;
    int _crt_slice = -1; // the first packet to be sent will cause a path update
    
    int directCount;

 protected:
    // start serving the item at the head of the queue
    virtual void beginService(); 
    // wrap up serving the item at the head of the queue
    virtual void completeService();

    PriorityQueue::queue_priority_t getPriority(Packet& pkt);
    list <Packet*> _queue[Q_NONE];
    mem_b _queuesize[Q_NONE];
    queue_priority_t _servicing;
    int _state_send;
};

#endif
