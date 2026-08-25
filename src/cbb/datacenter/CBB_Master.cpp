#include "CBB_Master.h"
#include "cbb_logging.h"
#include <algorithm>


// FIND AND RETURN VECTOR OF DIFFERENT INT VALUE FROM 2 SETS
vector<int> CBB_Master::find_different(unordered_set<int> Old, unordered_set<int> New) {
    unordered_set<int>::iterator it1;
    unordered_set<int>::iterator it2;
    vector<int> diff;
    for (it1 = Old.begin(); it1 != Old.end(); it1++) {
        int found = 0;
        for (it2 = New.begin(); it2 != New.end(); it2++) {
            if (*it1 == *it2) {
                found = 1;
                break;
            }
        }
        if (!found) {
            diff.push_back(*it1);
        }
    }
    return diff;
}



void CBB_Master::computeEmulatedConnectivity(int is_a2a) {
    // If connectivity hasn't been resized before, initialize it
    if (_emulated_connectivity.empty()) {
        _emulated_connectivity.resize(_ntor, vector<int>(_ntor, 0));
    }

    for (int i = 0; i < _ntor; i++) {
        for (int j = 0; j < _ntor; j++) {
            _emulated_connectivity[i][j] = 0;
        }
    }
    
    // Select the matching matrix based on a2a flag
    const IntMatrix& matching = (is_a2a) ? _matchingA2A : _matchingCBB;

    // Compute connectivity based on the selected matching matrix
    for (int i = 0; i < _ntor; i++) {
        for (int j = 0; j < (int)matching.size(); j++) {
            int nextToR = matching[j][i];
            _emulated_connectivity[i][nextToR] = 1;
            _emulated_connectivity[nextToR][i] = 1;
        }
    }
}


IntMatrix CBB_Master::calculateMatchingIndex() {

    // CREATE INDEX VECTOR
    IntMatrix matchingIndex;
    // RESIZE TO SIZE OF UPLINK
    matchingIndex.resize(_nul);
    for (int i = 0; i < _nul; i++) {
        // SPLIT FOR A2A AND CBB INDEX
        matchingIndex[i].resize(2);
        for (int j = 0; j < 2; j++) {
            matchingIndex[i][j] = -1; // SET DEFAULT VALUE TO -1
        }
    }

    // STORE _nCBB VALUE TO ANOTHER VARIABLE TEMPORARILY
    int temp = _nCBB;

    if (_is_a2a) {
        _nCBB = 0; // TEMPORARY SET _nCBB TO 0
    }

    int id = 0;
    int idHalf = 0;

    for (int i = 0; i < _nul; i++) {
        for (int j = 0; j < 2; j++) {
            // A2A
            if (j == 0) {
                matchingIndex[i][j] = id % (int)_matchingA2A.size();
            } // CBB
            else {
                matchingIndex[i][j] = idHalf % (int)_matchingCBB.size();    
            }
        }
        if (_nCBB != _nul) {
            id += 1;
        }
        if (i >= (_nul - _nCBB)) {
            idHalf += 1;
        }
    }


    _nCBB = temp;
    return matchingIndex;

}





void CBB_Master::union_lbls(const LabelPaths& lbls_old, const LabelPaths& lbls_new){
    int num_slice = lbls_old[0][0].size();
    for (int src = 0; src < _ntor; src++){
      for (int dst = 0; dst < _ntor; dst++){
        for (int slice = 0; slice < num_slice; slice++){
          
            int num_old_path = lbls_old[src][dst][slice].size();
            int num_new_path = lbls_new[src][dst][slice].size();
            int old_size = lbls_old[src][dst][slice][0].size();
            int new_size = lbls_new[src][dst][slice][0].size();
            
            int _lbls_start_slice = (superslice + slice)*3;

            

            if (old_size == 0 && new_size > 0){
                for (int i = 0; i < 3; i++){
                    _union_ref[src][dst][_lbls_start_slice + i] = 1;
                    _hop_number[src][dst][_lbls_start_slice + i] = new_size;
                    _paths_number[src][dst][_lbls_start_slice + i] = num_new_path;
                }
            }
            else if (new_size == 0 && old_size > 0){
                for (int i = 0; i < 3; i++){
                    _union_ref[src][dst][_lbls_start_slice + i] = 0;
                    _hop_number[src][dst][_lbls_start_slice + i] = old_size;
                    _paths_number[src][dst][_lbls_start_slice + i] = num_old_path;

                }
            }
            else{
                if (old_size < new_size){

                    ////// use old //////
                    for (int i = 0; i < 3; i++){
                    _union_ref[src][dst][_lbls_start_slice + i] = 0;
                    _hop_number[src][dst][_lbls_start_slice + i] = old_size;
                    _paths_number[src][dst][_lbls_start_slice + i] = num_old_path;
                    }
                }
                else if (old_size > new_size){

                    for (int i = 0; i < 3; i++){
                    _union_ref[src][dst][_lbls_start_slice + i] = 1;
                    _hop_number[src][dst][_lbls_start_slice + i] = new_size;
                    _paths_number[src][dst][_lbls_start_slice + i] = num_new_path;

                    }
                }
                else if (old_size == new_size){

                    // equal them union //
                    for (int i = 0; i < 3; i++){
                    _union_ref[src][dst][_lbls_start_slice + i] = 2;
                    _hop_number[src][dst][_lbls_start_slice + i] = new_size;
                    _paths_number[src][dst][_lbls_start_slice + i] = num_new_path + num_old_path;
                    }
                }

            }

        }
      }
    }
}





void CBB_Master::updateLabelPath_transition(map<string, int> operate){
    for (auto& pair : _new_CBB_lbls_library){
        remap_lbls(_mapping, pair.second);
    }

    if (operate["topo"] == 0){

        // change from a2a to cbb
        // remap cbb new

        // union
        if (_is_a2a == 1){
        _type_topo_before_transition = A2A_TOPOLOGY;
        _type_topo_after_transition = CBB_TOPOLOGY;
        union_lbls(_old_A2A_lbls_library[superslice], _new_CBB_lbls_library[superslice]);
        }
        else{
        _type_topo_before_transition = CBB_TOPOLOGY;
        _type_topo_after_transition = CBB_TOPOLOGY;
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << superslice << endl;
        union_lbls(_old_CBB_lbls_library[superslice], _new_CBB_lbls_library[superslice]);
        }
    }
    else if (operate["topo"] == 1 && _is_a2a == 0){

        // just union
        _type_topo_before_transition = CBB_TOPOLOGY;
        _type_topo_after_transition = A2A_TOPOLOGY;
        union_lbls(_old_CBB_lbls_library[superslice], _new_A2A_lbls_library[superslice]);
    }

}




void CBB_Master::remap_adj_lbps(const vector<int>& mapping){
    // mapping example if swap 2 to 5 then mapping = {0,1,5,3,4,2,6,7,...}
    // pair of swapping
    IntMatrix swaping_pair;
    vector<int> found;
    for (int i = 0; i < _ntor; i++){
        if (mapping[i] != i && !(find(found.begin(), found.end(), i) != found.end())){
            vector<int> pair = {i, mapping[i]};
            swaping_pair.push_back(pair);
            found.push_back(mapping[i]);
        }
    }

    // adj
    for (vector<int> pair : swaping_pair){
        for (int slice = 0; slice < _nslice; slice++){
            for (int port = 0; port < _ntor*_nul; port++){
                if (_adjacency_CBB[slice][port] == pair[0]){
                    _adjacency_CBB[slice][port] = pair[1];
                }
                else if (_adjacency_CBB[slice][port] == pair[1]){
                    _adjacency_CBB[slice][port] = pair[0];
                }
            }
            int src_start = pair[0]*_nul;
            int dst_start = pair[1]*_nul;
            for (int p = 0; p < _nul; p++){

                int temp = _adjacency_CBB[slice][dst_start + p];
                _adjacency_CBB[slice][dst_start + p] = _adjacency_CBB[slice][src_start + p];
                _adjacency_CBB[slice][src_start + p] = temp;
            }
        }
    }

    for (vector<int> pair : swaping_pair){
        for (int i = 0; i < _ntor; i++){
            if (i != pair[0] && i != pair[1]){
                swap(_lbls_CBB[i][pair[0]], _lbls_CBB[i][pair[1]]);
                swap(_lbls_CBB[pair[0]][i], _lbls_CBB[pair[1]][i]);
            }

        }

        for (int slice = 0; slice < _nslice; slice++){
            for (int path_index = 0; path_index < (int)_lbls_CBB[pair[0]][pair[1]][slice].size(); path_index++){
                auto& path = _lbls_CBB[pair[0]][pair[1]][slice][path_index];
                path = vector<int>(path.rbegin(), path.rend());
            }
        }

        for (int slice = 0; slice < _nslice; slice++){
            for (int path_index = 0; path_index < (int)_lbls_CBB[pair[1]][pair[0]][slice].size(); path_index++){
                auto& path = _lbls_CBB[pair[1]][pair[0]][slice][path_index];
                path = vector<int>(path.rbegin(), path.rend());
            }
        }

        for (int slice = 0; slice < _nslice; slice++){
            _lbls_CBB[pair[0]][pair[0]][slice] = {{}};
            _lbls_CBB[pair[1]][pair[1]][slice] = {{}};
        }
    }
}




void CBB_Master::remap_lbls(const vector<int>& mapping, LabelPaths& old_lbps){
    int num_slice = old_lbps[0][0].size();

    IntMatrix swaping_pair;
    vector<int> found;
    for (int i = 0; i < _ntor; i++){
        if (mapping[i] != i && !(find(found.begin(), found.end(), i) != found.end())){
            vector<int> pair = {i, mapping[i]};
            swaping_pair.push_back(pair);
            found.push_back(mapping[i]);
        }
    }
    // lbp[src][dst][slice][path_index][hop]
    for (vector<int> pair : swaping_pair){
        for (int i = 0; i < _ntor; i++){
            if (i != pair[0] && i != pair[1]){
                swap(old_lbps[i][pair[0]], old_lbps[i][pair[1]]);
                swap(old_lbps[pair[0]][i], old_lbps[pair[1]][i]);
            }
        }

        for (int slice = 0; slice < num_slice; slice++){
            for (int path_index = 0; path_index < (int)old_lbps[pair[0]][pair[1]][slice].size(); path_index++){
                auto& path = old_lbps[pair[0]][pair[1]][slice][path_index];
                path = vector<int>(path.rbegin(), path.rend());
            }
        }

        for (int slice = 0; slice < num_slice; slice++){
            for (int path_index = 0; path_index < (int)old_lbps[pair[1]][pair[0]][slice].size(); path_index++){
                auto& path = old_lbps[pair[1]][pair[0]][slice][path_index];
                path = vector<int>(path.rbegin(), path.rend());

            }
        }
        for (int slice = 0; slice < (int)old_lbps[pair[0]][pair[0]].size(); slice++){
            old_lbps[pair[0]][pair[0]][slice] = {{}};
            old_lbps[pair[1]][pair[1]][slice] = {{}};
        }
    }

}


void CBB_Master::calculate_mapping(const vector<int>& to_swap_x, const vector<int>& to_swap_y){
    if (cbb_logging::verbose_enabled()) {
        for (int i = 0; i < (int)to_swap_x.size(); i++){
            cbb_logging::debug() << to_swap_x[i] << " ";
        }
        cbb_logging::debug() << endl;
        for (int i = 0; i < (int)to_swap_y.size(); i++){
            cbb_logging::debug() << to_swap_y[i] << " ";
        }
        cbb_logging::debug() << endl;
    }
    _mapping.clear();
    _mapping.resize(_ntor);
    for (int i = 0; i < _ntor; i++)
        _mapping[i] = i;

    for (int i = 0; i < (int)to_swap_x.size(); i++){
        _mapping[to_swap_x[i]] = to_swap_y[i];
        _mapping[to_swap_y[i]] = to_swap_x[i];
    }
}



void CBB_Master::dfs_cc(int node, const IntMatrix& adjMatrix, vector<bool>& visited, unordered_set<int>& component) {
    visited[node] = true;
    component.insert(node);

    for (int neighbor = 0; neighbor < (int)adjMatrix[node].size(); ++neighbor) {
        if ((adjMatrix[node][neighbor] > 0 ||  adjMatrix[neighbor][node] > 0) && !visited[neighbor]) {
            dfs_cc(neighbor, adjMatrix, visited, component);
        }
    }
}

vector<unordered_set<int>> CBB_Master::Form_cc(const IntMatrix& adjMatrix) {
    int n = adjMatrix.size();
    vector<bool> visited(n, false);
    vector<unordered_set<int>> connectedComponents;

    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            unordered_set<int> component;
            dfs_cc(i, adjMatrix, visited, component);
            if (component.size() > 1)
                connectedComponents.push_back(component);
        }
    }

    return connectedComponents;
}


template <typename T, typename Container>
bool IN(const T& value, const Container& container) {
    for (const auto& element : container) {
        if (value == element) {
            return true;
        }
    }
    return false;
}

unordered_set<int> UNION(unordered_set<int> set_a, unordered_set<int> set_b){
    for(int i: set_b){
        set_a.insert(i);
    }
    return set_a;
}

unordered_set<int> DIFF(unordered_set<int> set_a, unordered_set<int> set_b){
    unordered_set<int> output;
    for (int i : set_a){
        if (IN(i,set_b) == false)
            output.insert(i);
    }
    return output;
}

map<string,int> CBB_Master::demand_aware_topology(IntMatrix to_swap){
    map<string,int> result;
    result["is_remap"] = 1;
    result["is_return_queue"] = 1;
    result["topo"] = 0;
    if (to_swap[0].size() == 0 && to_swap[1].size() == 0){
        result["is_remap"] = 0;
        result["is_return_queue"] = 0;
        return result;
    }
    if (to_swap[0][0] == -1){
        result["topo"] = 1;
        result["is_remap"] = 0;
        result["is_return_queue"] = 0;
    }
    return result;
}


IntMatrix CBB_Master::remapping_topo(int num_tor, const vector<int>& New_mapping, IntMatrix old_topo){
    IntMatrix new_topo;
    new_topo.resize(old_topo.size());

    for (int i = 0; i < (int)new_topo.size(); i++){
        new_topo[i].resize(num_tor);
        for (int j = 0; j < num_tor; j++){
            int old_src = j;
            int old_dst = old_topo[i][j];

            int new_src = New_mapping[old_src];
            int new_dst = New_mapping[old_dst];
            new_topo[i][new_src] = new_dst;
        }
    }
    return new_topo;
}






vector<int> CBB_Master::knapsack(const vector<int> &weights, const vector<int> &values, int capacity){
    // knapsack
    int n = values.size();
    IntMatrix dp(n + 1, vector<int>(capacity + 1, 0));
    // O(capacity * n) -> O(N^2)
    for (int i = 1; i < n + 1; i++){
        for (int w = 1; w < capacity + 1; w++){
            if (weights[i - 1] <= w)
                dp[i][w] = max(dp[i-1][w], dp[i - 1][w - weights[i - 1]] + values[i - 1]);
            else
                dp[i][w] = dp[i - 1][w];
        }
    }

    // Backtracking
    // O(N)
    int w = capacity;
    vector<int> index_result;

    for (int i = n; i > 0; i--){
        if (dp[i][w] != dp[i - 1][w]){
            index_result.push_back(i - 1);
            w -= weights[i - 1];
        }
    }

    return index_result;
}






IntMatrix CBB_Master::CBB_knapsack(int N, const unordered_set<int> &previous_half_x, const unordered_set<int> &previous_half_y, const vector<unordered_set<int>> &cc){

    vector<int> weighs, valuesX, valuesY, values;
    int sumX = 0;
    int sumY = 0;

    // O(cc_size * N/2) -> O(N^2)
    int sum_CC = 0;
    for (const unordered_set<int> &component : cc){
        int CC_size = component.size();
        if (CC_size > N/2){
            return IntMatrix {{-1}}; // return A2A
        }
        weighs.push_back(CC_size);
        sum_CC += CC_size;


        int wX = 0;
        int wY = 0;
        for (int c : component){
            bool inX = false;
            for (int pX : previous_half_x){
                if (c == pX){
                    inX = true;
                }
            }
            if (inX)
                wX += 1;
            else
                wY += 1;
        }
        sumX += wX;
        sumY += wY;
        valuesX.push_back(wX + 1);
        valuesY.push_back(wY + 1);
    }

    bool majorX = true;
    if (sumX >= sumY){
        values = valuesX;
        majorX = true;
    }
    else{
        values = valuesY;
        majorX = false;
    }

    // if sum_cc <= N/2
    // ...

    int capacity = N/2;

    // O(N^2)
    vector<int> result_index = knapsack(weighs, values, capacity);


    unordered_set<int> CC_X, CC_Y;


    for (int i = 0; i < (int)cc.size(); i++){
        bool inmajor = false;
        for (int idx: result_index){
            if (idx == i){
                inmajor = true;
            }
        }
        if (inmajor){
            if (majorX){
                CC_X = UNION(CC_X,cc[i]);
            }
            else{
                CC_Y = UNION(CC_Y,cc[i]);
            }
        }
        else{
            if (majorX){
                CC_Y = UNION(CC_Y,cc[i]);
            }
            else{
                CC_X = UNION(CC_X,cc[i]);
            }
        }
    }

    if ((int)CC_X.size() > N/2 || (int)CC_Y.size() > N/2){
        return IntMatrix {{-1}}; // return A2A
    }

    return knapsack_allocating(N,prev_half_x,prev_half_y,CC_X,CC_Y);
}





IntMatrix CBB_Master::knapsack_allocating(int, const unordered_set<int> &previous_half_x, const unordered_set<int> &previous_half_y, const unordered_set<int> &all_active_x, const unordered_set<int> &all_active_y){
    // cout << "start allocating" << endl;
    // find which nodes must swap and which nodes are free for swap
    unordered_set<int> to_swap_x = DIFF(all_active_x, previous_half_x);
    vector<int> free_x;
    for (int i : DIFF(previous_half_x, all_active_x)){
        free_x.push_back(i);
    }

    unordered_set<int> to_swap_y = DIFF(all_active_y, previous_half_y);
    vector<int> free_y;
    for (int i: DIFF(previous_half_y, all_active_y)){
        free_y.push_back(i);
    }

    // swap
    unordered_set<int> new_half_x = previous_half_x;
    unordered_set<int> new_half_y = previous_half_y;

    if ((int)to_swap_x.size() > (int)to_swap_y.size()){
        while ((int)to_swap_x.size() > (int)to_swap_y.size()){
            to_swap_y.insert(free_x[0]);
            free_x.erase(free_x.begin());
        }
    }
    else if ((int)to_swap_x.size() < (int)to_swap_y.size()){
        while ((int)to_swap_y.size() > (int)to_swap_x.size()){
            to_swap_x.insert(free_y[0]);
            free_y.erase(free_y.begin());
        }
    }

    new_half_x = UNION(DIFF(previous_half_x, to_swap_y), to_swap_x);
    new_half_y = UNION(DIFF(previous_half_y, to_swap_x), to_swap_y);


    ///////////////////////////////////////////////////////////////////////////
    // just print for debug
    vector<int> new_x;
    vector<int> new_y;
    vector<int> vec_to_swap_x;
    vector<int> vec_to_swap_y;

    if (to_swap_x.size() == 0) {

        for (int i: new_half_x) {
            new_x.push_back(i);
        }

        for (int i: new_half_y) {
            new_y.push_back(i);
        }

        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "total swap = " << to_swap_x.size() << endl;
        

        for(int i: to_swap_x){
            vec_to_swap_x.push_back(i);
        } 

        for(int i: to_swap_y){
            vec_to_swap_y.push_back(i);
        } 

    }

    else {

        std::vector<int> sorted_active_x(all_active_x.begin(), all_active_x.end());
        std::sort(sorted_active_x.begin(), sorted_active_x.end());
        if (cbb_logging::verbose_enabled()) {
            cbb_logging::debug() << "active_x:";
            for(int i : sorted_active_x){
                cbb_logging::debug() << " " << i;
            }
            cbb_logging::debug() << endl;
        }

        std::vector<int> sorted_active_y(all_active_y.begin(), all_active_y.end());
        std::sort(sorted_active_y.begin(), sorted_active_y.end());
        if (cbb_logging::verbose_enabled()) {
            cbb_logging::debug() << "active_y:";
            for (int i : sorted_active_y){
                cbb_logging::debug() << " " << i;
            }
            cbb_logging::debug() << endl;
        }

        std::vector<int> sorted_previous_half_x(previous_half_x.begin(), previous_half_x.end());
        std::sort(sorted_previous_half_x.begin(), sorted_previous_half_x.end());
        if (cbb_logging::verbose_enabled()) {
            cbb_logging::debug() << "previous half x: ";
            for (int i: sorted_previous_half_x) cbb_logging::debug() << i << " ";
            cbb_logging::debug() << endl;
        }

        std::vector<int> sorted_previous_half_y(previous_half_y.begin(), previous_half_y.end());
        std::sort(sorted_previous_half_y.begin(), sorted_previous_half_y.end());
        if (cbb_logging::verbose_enabled()) {
            cbb_logging::debug() << "previous half y: ";
            for (int i: sorted_previous_half_y) cbb_logging::debug() << i << " ";
            cbb_logging::debug() << endl;
        }

        std::vector<int> sorted_new_half_x(new_half_x.begin(), new_half_x.end());
        std::sort(sorted_new_half_x.begin(), sorted_new_half_x.end());
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "new half x: ";
        for (int i: sorted_new_half_x) {
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << i << " ";
            new_x.push_back(i);
        }
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << endl;

        std::vector<int> sorted_new_half_y(new_half_y.begin(), new_half_y.end());
        std::sort(sorted_new_half_y.begin(), sorted_new_half_y.end());
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "new half y: ";
        for (int i: sorted_new_half_y) {
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << i << " ";
            new_y.push_back(i);
        }
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << endl;


        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "total swap = " << to_swap_x.size() << endl;
        

        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "mapping : " << endl;
        for(int i: to_swap_x){
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << i << " ";
            vec_to_swap_x.push_back(i);
        } 
        if (cbb_logging::verbose_enabled()) {
            cbb_logging::debug() << endl;
            cbb_logging::debug() << "^" << endl;
            cbb_logging::debug() << "|" << endl;
            cbb_logging::debug() << "v" << endl;
        }
        for(int i: to_swap_y){
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << i << " ";
            vec_to_swap_y.push_back(i);
        } 
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << endl;

    }
    // cout << "finish allocating" << endl;
    return {vec_to_swap_x, vec_to_swap_y, new_x, new_y};
}
