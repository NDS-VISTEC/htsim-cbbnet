#ifndef DYNTOP
#define DYNTOP
#include <ostream>
#include <vector>
#include <set>
#include <map>
#include "main.h"
#include "eventlist.h"
#include <unordered_set>
#include "cbb_logging.h"
#include "CBB_Master.h"

#ifndef QT
#define QT
typedef enum {COMPOSITE} queue_type;
#endif




//class Logfile;
class Queue;
class Pipe;
class RlbModule;
class networkDevices;
class Controller;
class dynamicTopology: public EventSource, public CBB_Master {
    public:

        vector<vector<int>> get_lbls_path_full(int src, int dst, int slice);
        vector<int> get_lbls_path(int src, int dst, int slice){return get_lbls_path_full(src, dst, slice)[0];};
        int64_t get_nsuperslice() {return _nsuperslice;}
        int get_firstToR(int node) {return node / _ndl;}
        int get_lastport(int dst) {return dst % _ndl;}

        // defined in source file
        int get_nextToR(int slice, int crtToR, int crtport);
        int get_port(int srcToR, int dstToR, int slice, int path_ind, int hop){return get_lbls_path_full(srcToR, dstToR, slice)[path_ind][hop];}
        bool is_last_hop(int port);
        bool port_dst_match(int port, int crtToR, int dst);
        int get_no_paths(int srcToR, int dstToR, int slice);
        int get_no_hops(int srcToR, int dstToR, int slice, int path_ind);


        int failed_links;
        queue_type qt;

        dynamicTopology(mem_b queuesize, EventList& eventlist, queue_type q, string topfile, int interval, int delay_calculation, int delay_broadcast);
        // TimerDynamicTopo* _timerD;
        void doNextEvent();
        void dynamicTopo();
        
        void assignMatching();
        void assignMatching2(int sw);
        // void updateLabelPath(vector<int> adj);

        int no_of_nodes() const {return _no_of_nodes;} // number of servers
        int no_of_tors() const {return _ntor;} // number of racks
        int no_of_hpr() const {return _ndl;} // number of hosts per rack = number of downlinks
        int no_of_da() const {return _nCBB;}
        int no_of_upl() const {return _nul;}
        int is_a2a() const {return _is_a2a;}
        
        int get_nChanged(){return _nChanged;}

        int64_t getSuperSlice() const {return superslice;}
        uint64_t getSuperSliceCnt() const {return superslice_cnt;}
        int getMoveLocal() const {return moveToLocal;}  
        vector<int> get_different_nodes() const {return diff_nodes;}
        int get_emulated_connectivity(int src, int dst) const {return _emulated_connectivity[src][dst];}
        vector<vector<int>> get_full_emulated_connectivity() const {return _emulated_connectivity;}
        networkDevices* networkDev;

        void netDev(networkDevices* netDev) {
            networkDev = netDev; 
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Linked with networkDevices\n";
            doNextEvent();
        }

        void set_controller(Controller* con){ _controller = con; }

    
    
    private:
        map<string, int> _operate;
        vector<vector<int>> _result_map;
        int _delay_broadcast;
        int _delay_calculation;

        vector<vector<int>>  _adjacency_b;
        vector<vector<int>> _matchingStatic;
        vector<vector<vector<vector<int>>>> _pathStatic;
        mem_b _queuesize; // queue sizes
        uint64_t superslice_cnt;
        int moveToLocal;
        int moveToLocal_b;
        vector<int> changedSW;
        int sizeDA;
        int numFlowDA;

        vector<int> diff_nodes; 
        int _nChanged; // Number of remapped uplink
        int _current_uplink; // Current uplink
        int moveDelay;

        int _is_transition;
        int _start_superslice;
        string _fileprecomp_name;
        
        Controller* _controller;

};

#endif
