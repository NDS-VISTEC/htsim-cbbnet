#include "tor.h"
#include "rlbmodule.h"
#include "controller.h"
#include "networkDevices.h"
#include "compositequeue.h"
#include "network.h"
#include "ndp.h"
#include "cbb_logging.h"
// Logical Top of the Rack switch 
// the job of this ToR is just receive packet then sent this packet to output port
ToR::ToR(networkDevices* net, dynamicTopology* top, int tor_id, int ntor, int nul, int ndl){
    _tor_id = tor_id;
    _nul = nul;
    _ndl = ndl;
    _ntor = ntor;
    _path_index = vector<int>(_ntor, 0);
    queues_tor.resize(_nul + _ndl);
    local_rlb_moduls.resize(_ndl);
    _net = net;
    _top = top;
    _childen_ToRs.resize(2);
    _receiveNewMapping = false;
    _used_ports = vector<bool>(_nul,false);
}

void ToR::set_queue(int port, Queue* queue){
    queues_tor[port] = queue;
}

Queue* ToR::get_queue(int port) const{
    return queues_tor[port];
}

void ToR::set_local_rlb_module(int host, RlbModule* module){
    local_rlb_moduls[host] = module;
}


int ToR::get_output_port(Packet* pkt, int slice){
    int dst = 0;
    int dst_tor;
    int output_port = 0;
    if (pkt->type() == CBB_CNT && !pkt->is_to_controller()){
        dst_tor = pkt->get_dst();
    }
    else{
        dst = pkt->get_dst();
        dst_tor = dst/_ndl;
    }


    if (dst_tor == _tor_id){
        output_port = dst%_ndl;
        pkt->set_lasthop(true);
    }
    else{
        
        if (pkt->type() == RLB){
            // output_port = labls_path_full[0][0];
            for(int i = _ndl; i < _nul+_ndl; i++){
                if (_top->get_nextToR(slice, _tor_id, i) == dst_tor){
                    return i;
                }
            }
            if (cbb_logging::verbose_enabled()) {
                cbb_logging::debug() << "hello RLB: _tor_id = " << _tor_id << ", dst_tor = " << dst_tor << ", slice = " << slice << endl;
                cbb_logging::debug() << "pkt: real_src = " << pkt->get_real_src() << ", real_dst = " << pkt->get_real_dst() << ", src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << ", commit port = " << pkt->get_commit_port() << endl;
            }
        }

        else{
            vector<vector<int>> labls_path_full = _top->get_lbls_path_full(_tor_id, dst_tor, slice);

            int num_path_index = (int)labls_path_full.size();
            int crt_path_index = (_path_index[dst_tor] + 1)% num_path_index;
            output_port = labls_path_full[crt_path_index][0];
            _path_index[dst_tor] = crt_path_index; // round robin path index (ECMP)

            // if (pkt->type() == NDP || pkt->type() == NDPACK){
            //     if (pkt->get_ndpsrc()->get_flowid() == 3469 || pkt->get_ndpsrc()->get_flowid() == 45 || pkt->get_ndpsrc()->get_flowid() == 173){
            //         if (pkt->type() == NDP){cout << "\nToR " << get_tor_id() << " getting output port (NDP), num_path_index = " << num_path_index << ", crt_path_index = " << crt_path_index << ", output_port = " << output_port << ", at " << pkt->get_topology()->eventlist().now() << endl;}
                    
            //         if (pkt->type() == NDPACK){cout << "\nToR " << get_tor_id() << " getting output port (ACK), num_path_index = " << num_path_index << ", crt_path_index = " << crt_path_index << ", output_port = " << output_port << ", at " << pkt->get_topology()->eventlist().now() << endl;}
                    
            //         cout << "nChanged = " << _top->get_nChanged() << endl;
            //         for (int port=6; port<12; port++){
            //             cout << "ToR: " << get_tor_id() << ", port: " << port << ", connecting to ToR " << _top->get_nextToR(slice, get_tor_id(), port) << endl;
            //         }
            //         cout << "port to dst, from " << get_tor_id() << " to " << dst_tor << ": ";
            //         for (int hop=0; hop<(int)labls_path_full[crt_path_index].size(); hop++){
            //             cout << labls_path_full[crt_path_index][hop] << " " ;
            //         }
            //         cout << endl;
            //     }
            // }

        }
        output_port = output_port + _ndl - _nul;

    }
    return output_port;
};


void ToR::collect_demand(){
    _demands = vector<bool>(_ntor, false);
    bool has_demand = false;
    for (int host = 0; host < _ndl; host++){
        // for each destination ToR
        vector<int> queue_sizes = local_rlb_moduls[host]->get_local_queue_sizes();
        for (int dstToR = 0; dstToR < _ntor; dstToR++){
            for (int dstH = 0; dstH < _ndl; dstH ++){
                if (queue_sizes[dstToR*_ndl + dstH] > 0){
                    _demands[dstToR] = true;
                    has_demand = true;
                }
            }
        }
    }    
    _used_ports = vector<bool>(_nul, false);
    _receiveNewMapping = false;
    send_control_pkt(has_demand);

    // if (has_demand){
    //     // cout << "ToR " << _tor_id << " has demand" << endl;
    // }
}


// create and send the control pkt to central controller
void ToR::send_control_pkt(bool has_demand){
    // the controller locate at host 0
    // size is just 2 bytes
    ControlPacket* p;
    if (has_demand){
        p = ControlPacket::newpkt(_net, _top, _conSink, NULL, _tor_id, 0, _demand_pkt_size);
    }
    else{
        p = ControlPacket::newpkt(_net, _top, _conSink, NULL, _tor_id, 0, 1);
    }
    p->set_crtToR(_tor_id);
    p->set_crthop(0);
    p->to_controller(true);
    int output_port = get_output_port(p, _top->getSuperSlice()*3 + 2);
    p->set_crtport(output_port);

    queues_tor[output_port]->receivePacket(*p);
}

void ToR::receivePacket(Packet& pkt){
    if (pkt.type() == CBB_CNT && !pkt.is_to_controller()){
        int ingress_port = pkt.get_crtport();
        if (ingress_port >= _ndl){
            _used_ports[ingress_port - _ndl] = true;
        }
        pkt.free();
        _receiveNewMapping = true;
        for (int port = _ndl; port < _ndl + _nul; port++){
            if (!_used_ports[port - _ndl]){
                _used_ports[port - _ndl] = true;
                int nextToR = _top->get_nextToR(_top->getSuperSlice()*3 + 2, _tor_id, port);
                ControlPacket* p = ControlPacket::newpkt(_net, _top, NULL, NULL, _tor_id, nextToR, _mapping_plt_size);
                p->set_crtToR(_tor_id);
                p->set_crthop(0);
                p->to_controller(false);
                p->set_lasthop(false);
                p->set_crtport(port);
                queues_tor[port]->receivePacket(*p);

            }
        }
    }
    else{

        int port = get_output_port(&pkt, _top->getSuperSlice()*3);
        pkt.set_crtport(port);

        if (pkt.type() == RLB && pkt.get_real_dst() / _top->no_of_hpr() == pkt.get_real_src() / _top->no_of_hpr()) {
            pkt.set_crtport(_top->get_lastport(pkt.get_real_dst()));
            pkt.set_dst(pkt.get_real_dst());
        }

        else {

            if (port < _top->no_of_hpr()){
                pkt.set_lasthop(true);

            }
            else{

                if (pkt.type() == RLB && (pkt.get_commit_port() != pkt.get_crtport())) {
                    pkt.set_crtport(pkt.get_commit_port());

                }
            }
        }

        queues_tor[pkt.get_crtport()]->receivePacket(pkt);
    }
}
