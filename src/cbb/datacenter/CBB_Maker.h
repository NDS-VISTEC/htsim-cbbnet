#ifndef CBB_MAKER_H
#define CBB_MAKER_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <map>
#include "main.h"
#include "config.h"

using namespace std;

using IntMatrix = vector<vector<int>>;
using IntCube = vector<vector<vector<int>>>;
using PathTable = vector<vector<vector<vector<int>>>>;
using LabelPaths = vector<vector<vector<vector<vector<int>>>>>;
using LabelPathLibrary = map<int, LabelPaths>;


struct transition_Data
{
    int index_matching;
    int switch_id;
};






class CBB_Maker{
    public:
        void read_params(const string& topfile);
        simtime_picosec get_slicetime(int ind) {return _slicetime[ind];} // picoseconds spent in each slice
        IntMatrix cal_A2A_matchingIndex_list(int num_uplink, int num_tor);
        IntMatrix cal_CBB_matchingIndex_list(int num_uplink, int num_tor);
        // void read_adj_lbls(string adj_lbls_file);
        void read_adj_lbls(const string& adj_lbls_file);
        IntMatrix adjMaker(int num_uplink, int num_nodes, const IntMatrix &matchings, int type);
        LabelPaths lblsMaker(int num_downlink ,IntMatrix matchings, IntMatrix matching_index, IntMatrix adj_topo, int type, int num_sw);
        void write_adj_lbls(string outfile, const IntMatrix & _adj_A2A, const IntMatrix & _adj_CBB, const LabelPaths& _lbls_A2A, const LabelPaths& _lbls_CBB);
        void lbls_library();
        IntCube cal_matching_list(IntMatrix matchings);
        IntCube SingleSource_Kshortest(int src, int num_nodes, IntMatrix adj_matrix, IntMatrix neighbours_list , bool print);
        bool is_connected(IntMatrix adj_matrix);
        void dfs(IntMatrix& adj_matrix, vector<bool>& visited, int start);
        IntCube make_adj_from_temp_index(vector<vector<transition_Data>> temp_index, IntMatrix matchings,int ntor);
        LabelPaths lbls_transition(IntCube set_adj, vector<vector<transition_Data>> temp_index, IntMatrix matchings, int ndl);
        vector<vector<double>> to_cost_matrix(IntMatrix adj_matrix);
        vector<int> dijkstra(vector<vector<double>>& cost_matrix, int source, int target);


        vector<int> get_mapping(){ return _mapping_temp;}
        vector<int> get_mapping_back(){ return _mapping_back;}

        int _ndl, _nul, _ntor, _no_of_nodes; // number down links, number uplinks, number ToRs, number servers
        int _nA2A, _nCBB, _nStatic, _nslice, _interval;
        int64_t _nsuperslice; // number of "superslices" (periodicity of topology)
    
        vector<simtime_picosec> _slicetime; // picoseconds spent in each topology slice type (3 types)
        IntMatrix _matchingA2A, _matchingCBB;
        IntMatrix _matchingIndex_A2A, _matchingIndex_CBB;




        IntMatrix _adjacency_CBB;
        IntMatrix _adjacency_A2A;
        LabelPaths _lbls_CBB;
        LabelPaths _lbls_A2A;
        IntMatrix _adjacency;
        int _is_a2a;
        unordered_set<int> prev_half_x;
        unordered_set<int> prev_half_y;

        IntCube _union_ref; // 0 use old paths, 1 use new paths
        IntCube _paths_number;
        IntCube _hop_number;
        vector<int> _mapping;
        vector<int> _mapping_temp;
        vector<int> _mapping_back;

        LabelPathLibrary _old_CBB_lbls_library;
        LabelPathLibrary _new_CBB_lbls_library;
        LabelPathLibrary _old_A2A_lbls_library;
        LabelPathLibrary _new_A2A_lbls_library;
        
};


#endif
