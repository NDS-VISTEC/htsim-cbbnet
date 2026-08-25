// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "dynamicTopology.h"
#include <vector>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream> // to read from file
#include <filesystem>
#include <algorithm>

#include <climits>
#include <chrono>

#include "cbb_logging.h"
#include "main.h"
#include "queue.h"
#include "pipe.h"
#include "compositequeue.h"
#include "rlbmodule.h"
#include "controller.h"
extern uint32_t delay_host2ToR; // nanoseconds, host-to-tor link
extern uint32_t delay_ToR2ToR; // nanoseconds, tor-to-tor link
using namespace std;
string ntoa(double n);
string itoa(uint64_t n);


dynamicTopology::dynamicTopology(mem_b queuesize, EventList& eventlist, queue_type q, string topfile, int interval, int delay_calculation, int delay_broadcast) 
: EventSource(eventlist, "dynamic") {
    _queuesize = queuesize;
    qt = q;
    superslice = -1;
    superslice_cnt = -1;
    moveToLocal = 0;
    moveToLocal_b = -1;
    moveDelay = -1;
    _current_uplink = -1;
    _nChanged = 0;
    _is_a2a = 1;
    _interval = interval;
    _delay_calculation = delay_calculation;
    _delay_broadcast = delay_broadcast;
    read_params(topfile);
}

void dynamicTopology::doNextEvent() {
    dynamicTopo();

}




void dynamicTopology::dynamicTopo() {
    // INCREASE INDEX OF SUPERSLICE VALUE.
    superslice++; 
    superslice_cnt++;

    superslice = superslice % _nsuperslice;
    // INCREASE INDEX OF CURRENT UPLINK
    _current_uplink++;
    _current_uplink = _current_uplink % _nul;
    if (eventlist().now() == 0) {
        computeEmulatedConnectivity(_is_a2a);
    }

    else {
        int slice_now = superslice_cnt % _interval;
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "---------------------- slice now" << slice_now << endl;
        if (slice_now == _interval - 1 || slice_now == 0){
            // just check
            if (cbb_logging::verbose_enabled()) {
                cbb_logging::debug() << "superslice_cnt = " << superslice_cnt << ", interval = " << _interval << endl;
                cbb_logging::debug() << "check receviving" << endl;
            }
            for (int i = 0; i < _ntor; i++){
                ToR* tor = networkDev->get_top_of_rack(i);
                if (cbb_logging::verbose_enabled() && !tor->receiveNewMapping()){
                    cbb_logging::debug() << "tor " << i << " has not received newmapping" << endl;
                }
            }
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "fin check" << endl;
        }

        if (_nChanged != 0) { // TRANSITIONING

            _is_transition = 1;

            // ASSIGN MATCHING TO CURRENT SWITCH.

            if (_nChanged == _nul) {
                _is_transition = 0;
                // SET "_NCHANGED" TO 0.
                _nChanged = 0;

                // SET ADJACENCY AND LABEL PATH WITH TEMP VALUE WHEN "_NCHANGED" IS 0.
                _adjacency.assign(_adjacency_b.begin(), _adjacency_b.end());
                // remap old, I will ready to use for the next transition
                if (_is_a2a == 0){
                    for (auto& pair : _old_CBB_lbls_library){
                        remap_lbls(_mapping, pair.second);
                    }
                }
                computeEmulatedConnectivity(_is_a2a);
                // SET RECALCULATE SORTED NODE AND MOVE TO LOCAL Q
                //assert(moveToLocal_b != -1);
                if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "moveToLocal_b " << moveToLocal_b << endl;
                if (moveToLocal_b == 1) {
                    moveToLocal = 1;
                    moveDelay = 1;
                    moveToLocal_b = -1;
                }
            }

            else {
                // INCREASE "_NCHANGED" BY 1.
                _nChanged += 1;
            }
        }

        else if (slice_now == _interval - _delay_calculation) {
            if (cbb_logging::verbose_enabled()) {
                cbb_logging::debug() << "----------------------- calculation -----------------------" << endl;
                cbb_logging::debug() << "superslice_cnt = " << superslice_cnt << ", interval = " << _interval << endl;
                cbb_logging::debug() << "is_a2a = " << _is_a2a << endl;
            }
            _is_transition = 1;

      

            vector<unordered_set<int>> cc = Form_cc(_controller->demand_matrix); ///
            if (cbb_logging::verbose_enabled()) {
                cbb_logging::debug() << "cc_size " << cc.size() << endl;
                cbb_logging::debug() << "cc_len " ;
                for (unordered_set<int> c: cc) {
                    cbb_logging::debug() << c.size() << " ";
                }
                cbb_logging::debug() << endl;

                cbb_logging::debug() << "cc " ;
                for (unordered_set<int> c: cc) {
                    cbb_logging::debug() << "{";
                    for (int i: c){
                        cbb_logging::debug() << i << " " ;
                    }
                    cbb_logging::debug() << "}";
                }
                cbb_logging::debug() << endl;
            }
            _result_map = CBB_knapsack(_ntor, prev_half_x, prev_half_y, cc); ///

            _operate = demand_aware_topology(_result_map);

            _controller->demand_matrix = vector<vector<int>>(_ntor, vector<int>(_ntor, 0));
        }
        else if (slice_now == _interval - _delay_broadcast){
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "----------------------- sending broadcast pkt -----------------------" << endl;
            _controller->broadcast_update();
        }

        else if (slice_now == 0){
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "----------------------- new cycle -----------------------" << endl;
            _start_superslice = superslice;

            if (_operate["is_return_queue"]) {
                moveToLocal_b = 1;
            }
            else {
                moveToLocal = 0;
            }
            //topo remap returnq
            if ((_is_a2a == 1 && _operate["topo"] == 0) || _operate["is_remap"] == 1) { // CBB and remap
                calculate_mapping(_result_map[0], _result_map[1]);

                // REMAPPING MATCHINGS TOPOLOGY.
                vector<vector<int>> New_CBB_topo = remapping_topo(_ntor, _mapping, _matchingCBB);
                // CONVERT MATCHINGS TOPOLOGY TO CONNECTION MATRIX FORM.

                // ASSIGN NEW REMAPPING MATCHINGS TOPOLOGY. 
                _matchingCBB.assign(New_CBB_topo.begin(), New_CBB_topo.end());

                computeEmulatedConnectivity(_is_a2a);
                // CREATE NEW ADJACENCY AND LABEL PATH IN SLOT B.
                remap_adj_lbps(_mapping); // SWAP
                _adjacency_b = _adjacency_CBB;

                // SET SYSTEM TOPOLOGY MODE TO CBB.
                // union
                updateLabelPath_transition(_operate);
                _is_a2a = 0;

                // ASSIGN MATCHING TO CURRENT SWITCH.

                // INCREASE "_NCHANGED" BY 1.
                _nChanged += 1;
            }
            // CHECK IF THE SYSTEM WANT TO CHANGE SWITCH MODE TO A2A FROM CBB.
            else if (_operate["topo"] == 1 && _is_a2a == 0) {

                _adjacency_b = _adjacency_A2A;

                // SET SYSTEM TOPOLOGY MODE TO A2A.
                // union

                for (int i = 0; i < _ntor; i++){
                _mapping[i] = i;
                }
                updateLabelPath_transition(_operate);
                _is_a2a = 1;

                // ASSIGN MATCHING TO CURRENT SWITCH.
                computeEmulatedConnectivity(_is_a2a);

                // INCREASE "_NCHANGED" BY 1.
                _nChanged += 1;
            }

            // SET CURRENT DEMAND AWARE NODES CONNECTION MATRIX TO PREVIOUS ONE.
            if (_result_map.size() == 4) {
                prev_half_x.clear(); prev_half_y.clear();
                for (int i = 0; i < (int)_result_map[2].size(); i++) {
                    prev_half_x.insert(_result_map[2][i]);
                }
                for (int i = 0; i < (int)_result_map[3].size(); i++) {
                    prev_half_y.insert(_result_map[3][i]);
                }
            }

        }

        
    }


    if (moveDelay > 0){
        moveDelay -= 1; 
    }
    else{
        moveToLocal = 0;
    }


    // cout << _is_a2a << " DYNAMIC: Add time for next event\n\n";
    // SET UP THE RECONFIGURATION EVENT.
    eventlist().sourceIsPendingRel(*this, get_slicetime(3));
}



int dynamicTopology::get_nextToR(int slice, int crtToR, int crtport) {
    int uplink = crtport - _ndl + crtToR*_nul;
    
    if (_nChanged == 0){
        // cout << "normal" << endl;
        return _adjacency[slice][uplink];
    }

    else {
        if (crtport - _ndl >= _nChanged){
            // cout << "transition get old" << endl;
            return _adjacency[slice][uplink];
        }

        else {
            // cout << "transition get new" << endl;
            return _adjacency_b[slice][uplink];
        }
    }

}


bool dynamicTopology::is_last_hop(int port) {
    if ((port >= 0) && (port < _ndl)) // it's a ToR downlink
        return true;
    return false;
}

bool dynamicTopology::port_dst_match(int port, int crtToR, int dst) {
    if (port + crtToR*_ndl == dst)
        return true;
    return false;
}

int dynamicTopology::get_no_paths(int srcToR, int dstToR, int slice) {
    if (_nChanged == 0){
        if (_is_a2a){
            return _lbls_A2A[srcToR][dstToR][slice].size();
        }
        else{
            return _lbls_CBB[srcToR][dstToR][slice].size();
        }
    }
    else {
        return _paths_number[srcToR][dstToR][slice];
    }

}

int dynamicTopology::get_no_hops(int srcToR, int dstToR, int slice, int path_ind) {
    if (_nChanged == 0){
        if (_is_a2a){
            return _lbls_A2A[srcToR][dstToR][slice][path_ind].size();
        }
        else{
            return _lbls_CBB[srcToR][dstToR][slice][path_ind].size();
        }
    }
    else{
        return _hop_number[srcToR][dstToR][slice];
    }
}


vector<vector<int>> dynamicTopology::get_lbls_path_full(int src, int dst, int slice) {
    if (_nChanged == 0){
        if (_is_a2a){
            return _lbls_A2A[src][dst][slice];
        }
        else{
            return _lbls_CBB[src][dst][slice];
        }
    }

    // transition
    else{
        int to_superslice_idx = _nChanged - 1;
        int start_superslice = _start_superslice;

        // type apply = use old or new paths
        int type_apply = _union_ref[src][dst][slice];

        // if (getSuperSliceCnt() == 1836){cout << "_nChanged = " << _nChanged << ", type_apply = " << type_apply << ", start_superslice = " << start_superslice << ", _type_topo_before_transition = " << _type_topo_before_transition << endl;}
        if (type_apply == 0){
            if (_type_topo_before_transition == A2A_TOPOLOGY){
                return _old_A2A_lbls_library[start_superslice][src][dst][to_superslice_idx];
            }
            else{
                return _old_CBB_lbls_library[start_superslice][src][dst][to_superslice_idx];
            }
        }
        else if (type_apply == 1){
            if (_type_topo_after_transition == A2A_TOPOLOGY){
                return _new_A2A_lbls_library[start_superslice][src][dst][to_superslice_idx];
            }
            else{
                return _new_CBB_lbls_library[start_superslice][src][dst][to_superslice_idx];
            }
        }

        // else, combine paths
        else{
            // union 
            // count of old and new are equal.
            // int num_path = 0;

            //build temp to combine 
            if ((_type_topo_before_transition == A2A_TOPOLOGY) && (_type_topo_after_transition == CBB_TOPOLOGY)){
                vector<vector<int>> temp = _old_A2A_lbls_library[start_superslice][src][dst][to_superslice_idx];
                temp.insert(temp.end(), _new_CBB_lbls_library[start_superslice][src][dst][to_superslice_idx].begin(), _new_CBB_lbls_library[start_superslice][src][dst][to_superslice_idx].end());
                return temp;
            } 
            else if ((_type_topo_before_transition == CBB_TOPOLOGY) && (_type_topo_after_transition == CBB_TOPOLOGY)){
                vector<vector<int>> temp = _old_CBB_lbls_library[start_superslice][src][dst][to_superslice_idx];
                temp.insert(temp.end(), _new_CBB_lbls_library[start_superslice][src][dst][to_superslice_idx].begin(), _new_CBB_lbls_library[start_superslice][src][dst][to_superslice_idx].end());
                return temp;
            }
            else{
                vector<vector<int>> temp = _old_CBB_lbls_library[start_superslice][src][dst][to_superslice_idx];
                temp.insert(temp.end(), _new_A2A_lbls_library[start_superslice][src][dst][to_superslice_idx].begin(), _new_A2A_lbls_library[start_superslice][src][dst][to_superslice_idx].end());

                return temp;
            }
        }
    }
}
