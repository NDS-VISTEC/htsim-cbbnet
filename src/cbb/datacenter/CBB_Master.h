#ifndef CBB_MASTER_H
#define CBB_MASTER_H

#include "CBB_Maker.h"

// all algorithm must be in here
class CBB_Master: public CBB_Maker{
    public:
        vector<int> find_different(unordered_set<int> Old, unordered_set<int> New);
        void computeEmulatedConnectivity(int is_a2a);
        IntMatrix calculateMatchingIndex();
        void union_lbls(const LabelPaths& lbls_old, const LabelPaths& lbls_new);
        void updateLabelPath_transition(map<string, int> operate);
        void remap_adj_lbps(const vector<int>& mapping);
        void remap_lbls(const vector<int>& mapping, LabelPaths & old_lbps);
        void calculate_mapping(const vector<int>& to_swap_x, const vector<int>& to_swap_y);
        void dfs_cc(int node, const IntMatrix& adjMatrix, vector<bool>& visited, unordered_set<int>& component);
        vector<unordered_set<int>> Form_cc(const IntMatrix& adjMatrix);
        map<string,int> demand_aware_topology(IntMatrix to_swap);
        IntMatrix remapping_topo(int num_tor, const vector<int>& New_mapping, IntMatrix old_topo);
        IntMatrix _emulated_connectivity;
        int superslice;
        int _type_topo_before_transition;
        int _type_topo_after_transition;

        vector<int> knapsack(const vector<int> &weights, const vector<int> &values, int capacity);
        IntMatrix CBB_knapsack(int N, const unordered_set<int> &previous_half_x, const unordered_set<int> &previous_half_y, const vector<unordered_set<int>> &cc);
        IntMatrix knapsack_allocating(int N, const unordered_set<int> &previous_half_x, const unordered_set<int> &previous_half_y, const unordered_set<int> &all_active_x, const unordered_set<int> &all_active_y);

};

#endif
