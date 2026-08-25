#ifndef TCPPACKET_H
#define TCPPACKET_H

#include <list>
#include "network.h"

class TcpSrc;
class TcpSink;

// TcpPacket and TcpAck are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new TcpPacket or TcpAck directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

class TcpPacket : public Packet {
public:
	typedef uint64_t seq_t;

	inline static TcpPacket* newpkt(networkDevices* net, dynamicTopology* top, PacketFlow &flow, int src, int dst, TcpSrc* tcpsrc, TcpSink* tcpsink, seq_t seqno, seq_t dataseqno, int size) {
	    TcpPacket* p = _packetdb.allocPacket();
	    //p->set_route(flow,route,size,seqno+size-1); // The TCP sequence number is the first byte of the packet; I will ID the packet by its last byte.
	    p->set_attrs(flow, size, seqno+size-1, src, dst);
		p->set_topology(top);
		p->set_device(net);
		p->_type = TCP;
	    p->_seqno = seqno;
	    p->_data_seqno=dataseqno;
	    p->_syn = false;
		p->_tcpsrc = tcpsrc;
		p->_tcpsink = tcpsink;
	    return p;
	}

	inline static TcpPacket* newpkt(networkDevices* net, dynamicTopology* top, PacketFlow &flow, int src, int dst, TcpSrc* tcpsrc, TcpSink* tcpsink, seq_t seqno, int size) {
		return newpkt(net, top, flow, src, dst, tcpsrc, tcpsink, seqno, 0, size);
	}

	inline static TcpPacket* new_syn_pkt(networkDevices* net, dynamicTopology* top, PacketFlow &flow, int src, int dst, TcpSrc* tcpsrc, TcpSink* tcpsink, seq_t seqno, int size) {
		TcpPacket* p = newpkt(net, top, flow, src, dst, tcpsrc, tcpsink, seqno, 0, size);
		p->_syn = true;
		return p;
	}

	virtual inline TcpSrc* get_tcpsrc(){return _tcpsrc;}
    virtual inline TcpSink* get_tcpsink(){return _tcpsink;}

	void free() {_packetdb.freePacket(this);}
	virtual ~TcpPacket(){}
	inline seq_t seqno() const {return _seqno;}
	inline seq_t data_seqno() const {return _data_seqno;}
	inline simtime_picosec ts() const {return _ts;}
	inline void set_ts(simtime_picosec ts) {_ts = ts;}

	

protected:
	
	TcpSink* _tcpsink;
	TcpSrc* _tcpsrc;
	
	seq_t _seqno,_data_seqno;
	bool _syn;
	simtime_picosec _ts;
	static PacketDB<TcpPacket> _packetdb;
};

class TcpAck : public Packet {
public:
	typedef TcpPacket::seq_t seq_t;

	inline static TcpAck* newpkt(networkDevices* net, dynamicTopology* top, PacketFlow &flow, int src, int dst, TcpSrc* tcpsrc, seq_t seqno, seq_t ackno, seq_t dackno) {
	    TcpAck* p = _packetdb.allocPacket();
	    //p->set_route(flow,route,ACKSIZE,ackno);
		p->set_attrs(flow, 40, ackno, src, dst);
		p->set_topology(top);
    	p->set_device(net);
	    p->_type = TCPACK;
	    p->_seqno = seqno;
	    p->_ackno = ackno;
	    p->_data_ackno = dackno;
		p->_tcpsrc = tcpsrc;

	    return p;
	}

	inline static TcpAck* newpkt(networkDevices* net, dynamicTopology* top, PacketFlow &flow, int src, int dst, TcpSrc* tcpsrc, seq_t seqno, seq_t ackno) {
		return newpkt(net, top, flow, src, dst, tcpsrc, seqno, ackno, 0);
	}

	void free() {_packetdb.freePacket(this);}
	inline seq_t seqno() const {return _seqno;}
	inline seq_t ackno() const {return _ackno;}
	inline seq_t data_ackno() const {return _data_ackno;}
	inline simtime_picosec ts() const {return _ts;}
	inline void set_ts(simtime_picosec ts) {_ts = ts;}

	virtual ~TcpAck(){}
	//const static int ACKSIZE=40;

	virtual inline TcpSrc* get_tcpsrc(){return _tcpsrc;}

protected:

	TcpSrc* _tcpsrc;

	seq_t _seqno;
	seq_t _ackno, _data_ackno;
	simtime_picosec _ts;
	static PacketDB<TcpAck> _packetdb;
};

#endif
