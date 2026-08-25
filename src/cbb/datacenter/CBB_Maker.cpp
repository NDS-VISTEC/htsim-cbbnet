#include "CBB_Maker.h"
#include "cbb_logging.h"
#include <algorithm>
#include <filesystem>
#include <queue>
#include <set>
#include <stack>
#include <iterator>

using namespace std;

const double INF = numeric_limits<double>::infinity();

// READ THE TOPOLOGY INFORMATION FROM TEXT FILE
void CBB_Maker::read_params(const string& topfile) {

    _slicetime.resize(4);
    ifstream input(topfile);
    if (input.is_open()){
        // read the first line of basic parameters:
        string line;
        getline(input, line);
        stringstream stream(line);
        stream >> _no_of_nodes;
        stream >> _ndl;
        stream >> _nul;
        stream >> _ntor;

        _nCBB = _nul;

        // get number of topologies
        getline(input, line);
        stream.str(""); stream.clear(); // clear `stream` for re-use in this scope 
        stream << line;
        stream >> _nslice;
        // get picoseconds in each topology slice type
        _slicetime.resize(4);
        stream >> _slicetime[0]; // time spent in "epsilon" slice
        stream >> _slicetime[1]; // time spent in "delta" slice
        stream >> _slicetime[2]; // time spent in "r" slice
        // total time in the "superslice"
        _slicetime[3] = _slicetime[0] + _slicetime[1] + _slicetime[2];

        _nsuperslice = (_nslice / 3);

        assert((_nsuperslice % _interval == 0 || _interval % _nsuperslice == 0) && _interval > _nul);
        
        // READ A2A TOPOLOGY.
        _matchingA2A.resize(_ntor);
        for (int mat_idx = 0; mat_idx < _ntor; mat_idx++) {
            getline(input, line);
            stringstream stream(line);
            _matchingA2A[mat_idx].resize(_ntor); // Preallocate the inner vector
            for (int src = 0; src < _ntor; src++) {
                stream >> _matchingA2A[mat_idx][src];
            }
        }

        getline(input, line);

        // READ CBB TOPOLOGY.
        _matchingCBB.resize(_ntor/2);
        for (int mat_idx = 0; mat_idx < _ntor/2; mat_idx++) {
            getline(input, line);
            stringstream stream(line);
            _matchingCBB[mat_idx].resize(_ntor); // Preallocate the inner vector
            for (int src = 0; src < _ntor; src++) {
                stream >> _matchingCBB[mat_idx][src];
            }
        }

        getline(input, line);

        // CREATE ADJACENCY MATRIX AND LABEL PATH.
        // initial CBB and A2A adj, lbls
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Precalculation" << endl;
        // mathcingIndex[sw][slice] = index of matching that provide by the sw at that slice
        _matchingIndex_A2A = cal_A2A_matchingIndex_list(_nul, _ntor);
        _matchingIndex_CBB = cal_CBB_matchingIndex_list(_nul, _ntor);
        string fileprecomp_name = "../../topologies/precomp_"+ to_string(_ntor) + ".txt";
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << fileprecomp_name << endl;
        if (filesystem::exists(fileprecomp_name)){
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "already precompute" << endl;
            read_adj_lbls(fileprecomp_name);
        }
        else{
            if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start precompute" << endl;
            _adjacency_A2A = adjMaker(_nul, _ntor, _matchingA2A, A2A_TOPOLOGY);
            _lbls_A2A = lblsMaker(_ndl, _matchingA2A, _matchingIndex_A2A, _adjacency_A2A, A2A_TOPOLOGY, _nul);
            _adjacency_CBB = adjMaker(_nul, _ntor, _matchingCBB, CBB_TOPOLOGY);
            _lbls_CBB = lblsMaker(_ndl, _matchingCBB, _matchingIndex_CBB, _adjacency_CBB, CBB_TOPOLOGY, _nul); 
            write_adj_lbls(fileprecomp_name,_adjacency_A2A, _adjacency_CBB, _lbls_A2A, _lbls_CBB);
        }

        _adjacency = _adjacency_CBB;
        _is_a2a = 0;
        
        // _adjacency = _adjacency_A2A;
        // _is_a2a = 1;

        lbls_library(); // !!!!!!!!!!!!!!!!!!!!!!!!


        // SET ACTIVE NODES THAT START WITH NODE 0 TO NODE N/2. 
        for (int i = 0; i < _ntor; i++) {
            if (i < _ntor/2) {
                prev_half_x.insert(i);
            }
            else {
                prev_half_y.insert(i);
            }
        }

        _union_ref.resize(_ntor);
        _paths_number.resize(_ntor);
        _hop_number.resize(_ntor);
        for (int src = 0; src < _ntor; src++){
            _union_ref[src].resize(_ntor);
            _paths_number[src].resize(_ntor);
            _hop_number[src].resize(_ntor);
            for (int dst = 0; dst < _ntor; dst++){
                _union_ref[src][dst].resize(_nslice);
                _paths_number[src][dst].resize(_nslice);
                _hop_number[src][dst].resize(_nslice);
                for (int slice = 0; slice < _nslice; slice++){
                    _paths_number[src][dst][slice] = _lbls_CBB[src][dst][slice].size();
                    _hop_number[src][dst][slice] = _lbls_CBB[src][dst][slice][0].size();
                }
            }
        }

        _mapping.resize(_ntor);
        _mapping_temp.resize(_ntor);
        _mapping_back.resize(_ntor);
        for (int i=0; i < _ntor; i++) {
            _mapping[i] = i;
        }


    }
}



IntMatrix CBB_Maker::cal_A2A_matchingIndex_list(int num_uplink, int num_tor){
    int num_slice = num_tor*3;
    IntMatrix temp_A2A_matching_index;
    temp_A2A_matching_index.resize(num_uplink);
    for(int i = 0; i < num_uplink; i++){
        temp_A2A_matching_index[i].resize(num_slice);
        temp_A2A_matching_index[i][0] = i;
    }

    int switch_to_down = 0;
    for(int slice = 1; slice < num_slice; slice++){
        if (slice%3 == 0){
            for(int sw = 0; sw<num_uplink; sw++){
                if (temp_A2A_matching_index[sw][slice-1] == -1)
                    temp_A2A_matching_index[sw][slice] = (temp_A2A_matching_index[sw][slice-2] + num_uplink)%num_tor;
                else
                    temp_A2A_matching_index[sw][slice] = temp_A2A_matching_index[sw][slice-2];
            }
        }
        else if (slice%3 == 1){
            for(int sw = 0; sw < num_uplink; sw++){
                temp_A2A_matching_index[sw][slice] = temp_A2A_matching_index[sw][slice-1];
            }
        }
        else{
            for(int sw = 0; sw<num_uplink; sw++){
                if (sw != switch_to_down)
                    temp_A2A_matching_index[sw][slice] = temp_A2A_matching_index[sw][slice-1];
                else
                    temp_A2A_matching_index[sw][slice] = -1;

            }
            switch_to_down = (switch_to_down+1)%num_uplink;
        }
    }

    for(int sw = 0; sw < num_uplink; sw++){
        vector<int> temp_slice = temp_A2A_matching_index[sw];
        rotate(temp_slice.rbegin(), temp_slice.rbegin() + ((num_uplink-1)*3), temp_slice.rend());
        temp_A2A_matching_index[sw] = temp_slice;
    }
    return temp_A2A_matching_index;
}


IntMatrix CBB_Maker::cal_CBB_matchingIndex_list(int num_uplink, int num_tor){
    int n_CBB_matching = num_tor/2;
    int num_CBB_slice = n_CBB_matching*3;

    IntMatrix temp_CBB_matching_index;
    temp_CBB_matching_index.resize(num_uplink);
    for(int i = 0; i < num_uplink; i++){
        temp_CBB_matching_index[i].resize(num_CBB_slice);
        temp_CBB_matching_index[i][0] = i;
    }

    int switch_to_down = 0;
    for(int slice = 1; slice < num_CBB_slice; slice++){
        if (slice%3 == 0){
            for(int sw = 0; sw<num_uplink; sw++){
                if (temp_CBB_matching_index[sw][slice-1] == -1)
                    temp_CBB_matching_index[sw][slice] = (temp_CBB_matching_index[sw][slice-2] + num_uplink)%n_CBB_matching;
                else
                    temp_CBB_matching_index[sw][slice] = temp_CBB_matching_index[sw][slice-2];
            }
        }
        else if (slice%3 == 1){
            for(int sw = 0; sw < num_uplink; sw++){
                temp_CBB_matching_index[sw][slice] = temp_CBB_matching_index[sw][slice-1];
            }
        }
        else{
            for(int sw = 0; sw<num_uplink; sw++){
                if (sw != switch_to_down)
                    temp_CBB_matching_index[sw][slice] = temp_CBB_matching_index[sw][slice-1];
                else
                    temp_CBB_matching_index[sw][slice] = -1;

            }
            switch_to_down = (switch_to_down+1)%num_uplink;
        }
    }

    IntMatrix double_temp_CBB_matching_index = temp_CBB_matching_index;
    for(int sw = 0; sw < num_uplink; sw++){
        for (int slice = 0; slice < num_CBB_slice; slice++){
            double_temp_CBB_matching_index[sw].push_back(temp_CBB_matching_index[sw][slice]);
        }
    }

    for(int sw = 0; sw < num_uplink; sw++){
        vector<int> temp_slice = double_temp_CBB_matching_index[sw];
        rotate(temp_slice.rbegin(), temp_slice.rbegin() + ((num_uplink-1)*3), temp_slice.rend());
        double_temp_CBB_matching_index[sw] = temp_slice;
    }


    return double_temp_CBB_matching_index;
}



void CBB_Maker::read_adj_lbls(const string& adj_lbls_file) {
    // Open the file containing adjacency matrices and labels
    ifstream inputFile(adj_lbls_file);
    if (!inputFile.is_open()) {
        throw runtime_error("Unable to open adjacency labels file: " + adj_lbls_file);
    }

    string line;

    // Helper lambda to read adjacency matrices
    // - Reads adjacency matrix data for each slice into the provided vector.
    // - Assumes each slice's data is stored on a single line in the file.
    auto read_adjacency = [&](IntMatrix& adjacency) {
        adjacency.resize(_nslice); // Resize to hold all slices
        for (int slice = 0; slice < _nslice; slice++) {
            if (!getline(inputFile, line)) {
                throw runtime_error("Unexpected end of file while reading adjacency matrix.");
            }
            stringstream stream(line); // Tokenize the line
            adjacency[slice].resize(_ntor * _nul); // Resize for the number of TORs and uplinks
            for (int j = 0; j < _ntor * _nul; j++) {
                stream >> adjacency[slice][j]; // Extract each integer
            }
        }
    };

    // Helper lambda to read labels
    // - Reads labels for source-destination pairs and different slices.
    // - Labels for a source-destination pair are stored as a list of hops.
    auto read_labels = [&](LabelPaths& labels) {
        labels.resize(_ntor, PathTable(_ntor, vector<vector<vector<int>>>(_nslice)));
        int slice = 0;
        bool check = true;

        while (check && getline(inputFile, line)) {
            istringstream iss(line);
            vector<int> tokens; // Tokens store integers from the current line
            int num;
            while (iss >> num) {
                tokens.push_back(num); // Extract all integers
            }
            if (tokens.empty()) continue; // Skip empty lines

            if (tokens[0] == -1) {
                // End of labels section marked by -1
                check = false;
            } else if (tokens.size() == 1) {
                // A single integer represents the slice index
                slice = tokens[0];
            } else {
                // First two tokens are source and destination; the rest are hops
                int src = tokens[0], dst = tokens[1];
                vector<int> hops(tokens.begin() + 2, tokens.end());
                for (int i = 0; i < 3; i++){
                    labels[src][dst][slice+i].push_back(hops); // Add hops for this src-dst-slice
                }
            }
        }
    };

    // Read adjacency matrices and labels for A2A (all-to-all) topology
    read_adjacency(_adjacency_A2A);  // Populate _adjacency_A2A
    read_labels(_lbls_A2A);          // Populate _lbls_A2A

    // Read adjacency matrices and labels for CBB (core-backbone) topology
    read_adjacency(_adjacency_CBB);  // Populate _adjacency_CBB
    read_labels(_lbls_CBB);          // Populate _lbls_CBB

    // Ensure diagonal labels are initialized for self-connections
    for (int i = 0; i < _ntor; i++) {
        for (int slice = 0; slice < _nslice; slice++) {
            _lbls_A2A[i][i][slice].emplace_back(); // Add an empty vector for self-connections
            _lbls_CBB[i][i][slice].emplace_back(); // Add an empty vector for self-connections
        }
    }

    inputFile.close(); // Close the file after reading all data
}


// type A2A = 0, CBB = 1
IntMatrix CBB_Maker::adjMaker(int num_uplink, int num_nodes, const IntMatrix &matchings, int type){
    
    IntMatrix mat_index;
    if (type == A2A_TOPOLOGY)
        mat_index = cal_A2A_matchingIndex_list(num_uplink, num_nodes);
    else
        mat_index = cal_CBB_matchingIndex_list(num_uplink, num_nodes);
    
    int num_up_ports = num_nodes*num_uplink;
    int num_slices = (int)mat_index[0].size();

    IntMatrix adj_topo;
    adj_topo.resize(num_slices);

    int index;
    int start_index;
    int current_port;
    for(int slice = 0; slice < num_slices; slice++){
        adj_topo[slice].resize(num_up_ports);
        for (int node = 0; node < num_nodes; node++){
            start_index = node*num_uplink;
            for(int sw = 0; sw < num_uplink; sw++){
                current_port = start_index + sw;
                index = mat_index[sw][slice];
                if (index != -1)
                    adj_topo[slice][current_port] = matchings[ index ][node];
                else
                    adj_topo[slice][current_port] = -1;
            }
        }
    }
    return adj_topo;
}





LabelPaths CBB_Maker::lblsMaker(int num_downlink, IntMatrix matchings, IntMatrix matching_index, IntMatrix adj_topo, int type, int num_sw){
    int num_node = matchings[0].size();
    int num_slice = matching_index[0].size();
    // list of matchings
    // wheere node i connected to nodes j if matching_list[x][i][j] = 1
    IntCube matching_list = cal_matching_list(matchings);

    // sum all matching in each slice to be topology per slice bssed on matching index
    // collect neigbours_list : will be used at calculating shortest path stage
    IntCube topo_p_slice;
    IntCube neighbours_list;

    int temp_num;
    int temp_index;
    for (int slice = 0; slice < num_slice; slice++){
        IntMatrix topo;
        topo.resize(num_node);

        IntMatrix neighbours;
        neighbours.resize(num_node);

        for (int i = 0; i < num_node; i++){
            topo[i].resize(num_node);
            vector<int> neighbours_of_node_i;
            for (int j = 0; j < num_node; j++){
                temp_num = 0;
                for (int sw = 0; sw < num_sw; sw++){
                    temp_index = matching_index[sw][slice];
                    if (temp_index != -1){
                        if (matching_list[temp_index][i][j] > 0){
                            temp_num = temp_num + matching_list[temp_index][i][j];
                            neighbours_of_node_i.push_back(j);
                        }
                    }
                }
                topo[i][j] = temp_num;
            }
            neighbours[i] = neighbours_of_node_i;
        }
        topo_p_slice.push_back(topo);
        neighbours_list.push_back(neighbours);
    }


    // labelPath[src][dst][slice][path][hop]
    LabelPaths lbls;
    lbls.resize(num_node);
    for (int i = 0; i < num_node; i++){
        lbls[i].resize(num_node);
        for (int j = 0; j < num_node; j++)
            lbls[i][j].resize(num_slice);
    }

    int from_node;
    int to_node;
    int up_port;

    LabelPaths all_path;
    for (int slice = 0; slice < num_slice; slice++){
        PathTable path_per_slice;
        for (int src = 0; src < num_node; src++){
            IntCube path = SingleSource_Kshortest(src, num_node, topo_p_slice[slice], neighbours_list[slice], false);
            path_per_slice.push_back(path);
        }
        all_path.push_back(path_per_slice);
    }


    // always use path at down time
    for (int slice = 2; slice < num_slice; slice = slice + 3){
        all_path[slice-1] = all_path[slice];
        all_path[slice-2] = all_path[slice];
    }

    // labelPath[src][dst][slice][path][hop]
    for (int src = 0; src < num_node; src++){
        for (int dst = 0; dst < num_node; dst++){
            for (int slice = 0; slice < num_slice; slice++){
                int num_path_index = all_path[slice][src][dst].size();
                lbls[src][dst][slice].resize(num_path_index);
                for(int path_index = 0; path_index < num_path_index; path_index++){
                    vector<int> temp_path;

                    for (int hop = 0; hop < (int)all_path[slice][src][dst][path_index].size() - 1; hop++){
                        from_node = all_path[slice][src][dst][path_index][hop];
                        to_node = all_path[slice][src][dst][path_index][hop+1];
                        up_port = -1;
                        for (int sw = 0; sw < num_sw; sw++){
                            temp_index = matching_index[sw][slice];
                            if (temp_index != -1){
                                if (matchings[temp_index][to_node] == from_node){
                                    // Port of Tor is downlink + uplink (start from downlink)
                                    up_port = num_downlink + sw;
                                }
                            }
                        }
                        temp_path.push_back(up_port);
                    }
                    lbls[src][dst][slice][path_index] = temp_path;
                }
            }
        }
    }
    return lbls;
}




void CBB_Maker::write_adj_lbls(string outfile, const IntMatrix& _adj_A2A, const IntMatrix& _adj_CBB,
    const LabelPaths& _lbls_A2A, const LabelPaths& _lbls_CBB) {

    ofstream outputFile(outfile);
    
    if (outputFile.is_open()) {

        // Lambda to write adjacency list
        auto writeAdjList = [&outputFile](const IntMatrix& adjList) {
            for (const auto& slice : adjList) {
                for (size_t i = 0; i < slice.size(); ++i) {
                    outputFile << slice[i];
                    if (i < slice.size() - 1) outputFile << " ";
                    else outputFile << "\n";
                }
            }
        };

        // Lambda to write label information
        auto writeLbls = [&outputFile](const LabelPaths& lbls) {
            int nslice = lbls[0][0].size();
            int ntor = lbls.size();
            for (int slice = 0; slice < nslice; slice += 3) {
                outputFile << slice << "\n";
                for (int src = 0; src < ntor; ++src) {
                    for (int dst = 0; dst < ntor; ++dst) {
                        if (src != dst) {
                            int npath = lbls[src][dst][slice].size();
                            for (int path = 0; path < npath; ++path) {
                                int nhop = lbls[src][dst][slice][path].size();
                                outputFile << src << " " << dst << " ";
                                for (int hop = 0; hop < nhop; ++hop) {
                                    outputFile << lbls[src][dst][slice][path][hop];
                                    if (hop < nhop - 1) outputFile << " ";
                                    else outputFile << "\n";
                                }
                            }
                        }
                    }
                }
            }
            outputFile << -1 << "\n";
        };

        // Write _adj_A2A, _lbls_A2A, _adj_CBB, and _lbls_CBB
        writeAdjList(_adj_A2A);
        writeLbls(_lbls_A2A);
        writeAdjList(_adj_CBB);
        writeLbls(_lbls_CBB);

        outputFile.close();
    } else {
        cbb_logging::error() << "Error opening file: " << outfile << endl;
    }
}





void CBB_Maker::lbls_library(){
    // the superslice that will be remap
    vector<int> trigger_superslices;
    // what if interval > superslice ??? 
    for (int ss = 0; ss < _nsuperslice; ss++){ 
        if (ss%_interval == 0)
            trigger_superslices.push_back(ss);
    }

    for (int start_superslice : trigger_superslices){
        vector<vector<transition_Data>> temp_old_CBB_index;
        vector<vector<transition_Data>> temp_new_CBB_index;
        vector<vector<transition_Data>> temp_old_A2A_index;
        vector<vector<transition_Data>> temp_new_A2A_index;
        temp_old_CBB_index.resize(_nul);
        temp_new_CBB_index.resize(_nul);
        temp_old_A2A_index.resize(_nul);
        temp_new_A2A_index.resize(_nul);
        int slice;
        vector<int> sw_down_CBB(1, 0);
        vector<int> sw_down_A2A(1, 0);
        for (int step = 0; step < _nul; step++){
            // cout << "step = " << step << endl;
            slice = (start_superslice + step)*3 + 2; // down slice
            for (int sw = 0; sw < _nul; sw++){
                // cout << "sw = " << sw << endl;
                transition_Data temp_CBB = {_matchingIndex_CBB[sw][slice], sw};
                transition_Data temp_A2A = {_matchingIndex_A2A[sw][slice], sw};

                if (temp_CBB.index_matching == -1){
                    // cout << "sw down CBB = " << sw << endl;
                    sw_down_CBB.push_back(sw);
                }
                if (temp_A2A.index_matching == -1){
                    sw_down_A2A.push_back(sw);
                }

                bool sw_CBB_was_down = false;
                bool sw_A2A_was_down = false;
                for (int s: sw_down_CBB){
                    if (sw == s){
                        sw_CBB_was_down = true;
                    }
                }
                for (int s: sw_down_A2A){
                    if (sw == s){
                        sw_A2A_was_down = true;
                    }
                }

                if (temp_CBB.index_matching != -1){
                    if (sw_CBB_was_down){
                        // cout << "put in new" << endl;
                        temp_new_CBB_index[step].push_back(temp_CBB);
                    }
                    else{
                        // cout << "put in old" << endl;
                        temp_old_CBB_index[step].push_back(temp_CBB);
                    }
                }

                if (temp_A2A.index_matching != -1){
                    if (sw_A2A_was_down){
                        temp_new_A2A_index[step].push_back(temp_A2A);
                    }
                    else{
                        temp_old_A2A_index[step].push_back(temp_A2A);
                    }
                }

            }
        }



        // for each type we need adj and lbls
        
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start cal set_adj_old_CBB" << endl;
        IntCube set_adj_old_CBB = make_adj_from_temp_index(temp_old_CBB_index, _matchingCBB, _ntor);
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start cal set_adj_new_CBB" << endl;
        IntCube set_adj_new_CBB = make_adj_from_temp_index(temp_new_CBB_index, _matchingCBB, _ntor);
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start cal set_adj_old_A2A" << endl;
        IntCube set_adj_old_A2A = make_adj_from_temp_index(temp_old_A2A_index, _matchingA2A, _ntor);
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start cal set_adj_new_A2A" << endl;
        IntCube set_adj_new_A2A = make_adj_from_temp_index(temp_new_A2A_index, _matchingA2A, _ntor);

        // calculate lbls for each transition index type; change the name !!! 
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start cal _old_CBB_lbls_library" << endl;
        _old_CBB_lbls_library[start_superslice] = lbls_transition(set_adj_old_CBB, temp_old_CBB_index, _matchingCBB, _nul);
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start cal _new_CBB_lbls_library" << endl;
        _new_CBB_lbls_library[start_superslice] = lbls_transition(set_adj_new_CBB, temp_new_CBB_index, _matchingCBB, _nul);
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start cal _old_A2A_lbls_library" << endl;
        _old_A2A_lbls_library[start_superslice] = lbls_transition(set_adj_old_A2A, temp_old_A2A_index, _matchingA2A, _nul);
        if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "start cal _new_A2A_lbls_library" << endl;
        _new_A2A_lbls_library[start_superslice] = lbls_transition(set_adj_new_A2A, temp_new_A2A_index, _matchingA2A, _nul);

        
        // union it later
    }

    // auto& vec = _new_CBB_lbls_library[0];
    // for (size_t src = 0; src < vec.size(); ++src) {
    //     for (size_t dst = 0; dst < vec[src].size(); ++dst) {
    //         // for (size_t slice = 0; slice < vec[src][dst].size(); ++slice) {
    //         for (size_t slice = 0; slice < 1; ++slice) {
    //             std::cout << "lbls[" << src << "][" << dst << "][" << slice << "]: ";
    //             for (size_t hop = 0; hop < vec[src][dst][slice].size(); ++hop) {
    //                 std::cout << "[ ";
    //                 for (const auto& val : vec[src][dst][slice][hop]) {
    //                     std::cout << val << " ";
    //                 }
    //                 std::cout << "] ";
    //             }
    //             std::cout << std::endl;
    //         }
    //     }
    // }
}



IntCube CBB_Maker::cal_matching_list(IntMatrix matchings){
    int num_nodes = matchings[0].size();
    IntCube matching_list;

    for(int mat = 0; mat < (int)matchings.size(); mat++){
        IntMatrix matching;
        matching.resize(num_nodes);
        for(int i = 0; i < num_nodes; i++){
            matching[i].resize(num_nodes);
            for(int j = 0; j<num_nodes; j++){
                if (j == matchings[mat][i]){
                    matching[i][j] = 1;
                }
                else{
                    matching[i][j] = 0;
                }
            }
        }
        matching_list.push_back(matching);
    }
    return matching_list;
}




struct qData
{
    int node;
    vector<int> sub_path;
    qData(int n, vector<int> s) : node(n), sub_path(s) {}
};



// can support non connected graph
IntCube CBB_Maker::SingleSource_Kshortest(int src, int num_nodes, IntMatrix adj_matrix, IntMatrix neighbours_list, bool print){
    print = false;
    if (print){
        cbb_logging::debug() << "is this topo connected: " << is_connected(adj_matrix) << endl;
        for (int i = 0; i<num_nodes; i++){
            for(int j = 0; j<num_nodes; j++){
                cbb_logging::debug() << adj_matrix[i][j] << " ";
            }
            cbb_logging::debug() << endl;
        }
    }


    vector<vector<double>> cost_matrix = to_cost_matrix(adj_matrix);

    vector<int> min_size;
    for (int dst = 0; dst < num_nodes; dst++){
        min_size.push_back((int)dijkstra(cost_matrix, src, dst).size()); // !!! re-consider
    }




    int num_connected = 0;
    vector<bool> visited_(num_nodes, false);
    dfs(adj_matrix, visited_, src);
    for (bool vertex_visited : visited_) {
        if (vertex_visited) num_connected++;
    }
    if (print){
        cbb_logging::debug() << "num connected " << num_connected << endl;
    }
    queue<qData> q;
    qData init(src,{src});
    q.push(init);
    
    set<int> visited = {src};
    vector<int> temp_visited;

    IntCube Path_list;
    Path_list.resize(num_nodes);
    for (int i = 0; i < num_nodes; i++){
        if (!visited_[i]){
            Path_list[i] = {{-1}};
        }
    }

    Path_list[src] = {{src}};

    while ((int)visited.size() < num_connected || (int)q.size() > 0)
    {
        qData current = q.front();
        q.pop();
        
        int now = current.node;
        vector<int> path = current.sub_path;
        if (!(visited.find(now) != visited.end())){
            visited.insert(now);
        }

        for (int i : neighbours_list[now]){
            vector<int> new_path = path;
            new_path.push_back(i);
            if ((int)new_path.size() <= min_size[i]){
                if ((int)visited.size() < num_connected){
                    qData temp_data(i,new_path);
                    q.push(temp_data);
                }
                if ((int)Path_list[i].size() == 0){
                    Path_list[i].push_back(new_path);
                }
                else{
                    if ((int)new_path.size() == (int)Path_list[i][0].size()){
                        Path_list[i].push_back(new_path);
                    }
                }
            }
        }
    }
    
    return Path_list;
}


bool CBB_Maker::is_connected(IntMatrix adj_matrix){
    int num_vertices = adj_matrix.size();

    if (num_vertices == 0) {
        return false; // No vertices in the graph
    }

    vector<bool> visited(num_vertices, false);

    // Start DFS from the first vertex (you can start from any vertex)
    dfs(adj_matrix, visited, 0);
    // Check if all vertices are visited
    for (bool vertex_visited : visited) {
        if (!vertex_visited) {
            return false; // Graph is not connected
        }
    }

    return true; // Graph is connected
}


void CBB_Maker::dfs(IntMatrix& adj_matrix, vector<bool>& visited, int start) {
    int vertices = adj_matrix.size();
    stack<int> s;
    s.push(start);

    while (!s.empty()) {
        int current_vertex = s.top();
        s.pop();

        if (!visited[current_vertex]) {
            visited[current_vertex] = true;
        }

        for (int i = 0; i < vertices; i++) {
            if (adj_matrix[current_vertex][i] == 1 && !visited[i]) {
                s.push(i);
            }
        }
    }
}





vector<vector<double>> CBB_Maker::to_cost_matrix(IntMatrix adj_matrix){
    int num_nodes = adj_matrix.size();
    vector<vector<double>> cost_matrix;
    cost_matrix.resize(num_nodes);
    for(int i = 0; i < num_nodes; i++){
        cost_matrix[i].resize(num_nodes);
        for(int j = 0 ;j < num_nodes; j++){
            if (i == j){
                cost_matrix[i][j] = 0;
            }
            else if (adj_matrix[i][j] == 1){
                cost_matrix[i][j] = 1;
            }
            else{
                cost_matrix[i][j] = INF;
            }
        }
    }
    return cost_matrix;
}






IntCube CBB_Maker::make_adj_from_temp_index(vector<vector<transition_Data>> temp_index, IntMatrix matchings,int ntor){
    
    IntCube adj_matrix_slice;
    IntMatrix adj_matrix;

    adj_matrix.resize(ntor);
    for (int i = 0; i<ntor; i++){
        adj_matrix[i].resize(ntor);
        for (int j = 0; j < ntor; j++){
            adj_matrix[i][j] = 0;
        }
    }

    for(int slice = 0; slice < (int)temp_index.size(); slice++){
        adj_matrix_slice.push_back(adj_matrix);
    }

    for (int slice = 0; slice < (int)temp_index.size(); slice++){
        for (int sw = 0; sw < (int)temp_index[slice].size(); sw ++){
            int index = temp_index[slice][sw].index_matching;
            for (int src = 0; src < ntor; src++){
                int dst = matchings[index][src];
                adj_matrix_slice[slice][src][dst] = 1;
            }
        }
    }
    return adj_matrix_slice;
}





LabelPaths CBB_Maker::lbls_transition(IntCube set_adj, vector<vector<transition_Data>> temp_index, IntMatrix matchings, int ndl){
    int n_slice = set_adj.size();
    int ntor = set_adj[0].size();

    LabelPaths result_lbls;
    result_lbls.resize(ntor);
    for (int i = 0; i < ntor; i++){
        result_lbls[i].resize(ntor);
        for (int j = 0; j < ntor; j++){
            result_lbls[i][j].resize(n_slice);
        }
    }


    for(int slice = 0; slice < n_slice; slice++){
        IntMatrix neighbours_list;
        neighbours_list.resize(ntor);
        for (int i = 0; i < ntor; i++){
            vector<int> temp;
            for (int j = 0; j < ntor; j++){
                if (set_adj[slice][i][j] == 1)
                    temp.push_back(j);
            }
            neighbours_list[i] = temp;
        }

        // cout << "slice = " << slice << endl;
        // for (int src=0; src < ntor; src++){
            // cout << "src = " << src << ": nb = [ ";
            // for (const auto& nb : neighbours_list[src]) {
                // std::cout << nb << " ";
            // }
            // cout << "] " << endl;;
        // }
        // cout << endl;

        // k-shortest path
        PathTable K_shortest_path;
        K_shortest_path.resize(ntor);
        for(int src = 0; src < ntor; src++){
            // ss[dst][path_index][hop]
            IntCube ss = SingleSource_Kshortest(src, ntor, set_adj[slice], neighbours_list, true);
            K_shortest_path[src] = ss;
        }

        int from_node;
        int to_node;
        int up_port;
        for (int src = 0; src < ntor; src++){
            for (int dst = 0; dst < ntor; dst++){
                for (int path = 0; path < (int)K_shortest_path[src][dst].size(); path++){
                    if (K_shortest_path[src][dst][path][0] != -1){
                        vector<int> temp_path;
                        for (int hop = 0; hop < (int)K_shortest_path[src][dst][path].size()-1; hop++){
                            from_node = K_shortest_path[src][dst][path][hop];
                            to_node = K_shortest_path[src][dst][path][hop+1];
                            up_port = -1;
                            for (int i = 0; i < (int)temp_index[slice].size(); i++){
                                int index = temp_index[slice][i].index_matching;
                                int sw = temp_index[slice][i].switch_id;
                                if (matchings[index][to_node] == from_node){
                                    up_port = ndl + sw; //////////////////////////////////////
                                }
                            }
                            temp_path.push_back(up_port);
                        }
                        result_lbls[src][dst][slice].push_back(temp_path);
                    }
                    else{
                        result_lbls[src][dst][slice] = {{}};
                    }
                }
            }
        }
    }
    return result_lbls;
}





vector<int> CBB_Maker::dijkstra(vector<vector<double>>& cost_matrix, int source, int target) {
    int num_nodes = cost_matrix.size();
    vector<double> distances(num_nodes, INF);
    vector<int> previous_nodes(num_nodes, -1);
    distances[source] = 0;
    priority_queue<pair<double, int>, vector<pair<double, int>>, greater<pair<double, int>>> pq;
    pq.push({0, source});

    while (!pq.empty()) {
        double current_distance = pq.top().first;
        int current_node = pq.top().second;
        pq.pop();

        if (current_node == target) {
            vector<int> path;
            while (current_node != -1) {
                path.insert(path.begin(), current_node);
                current_node = previous_nodes[current_node];
            }
            return path;
        }

        for (int neighbor = 0; neighbor < num_nodes; ++neighbor) {
            double weight = cost_matrix[current_node][neighbor];
            if (weight != INF) {
                double distance = current_distance + weight;

                if (distance < distances[neighbor]) {
                    distances[neighbor] = distance;
                    previous_nodes[neighbor] = current_node;
                    pq.push({distance, neighbor});
                }
            }
        }
    }

    return vector<int>();  // Target node is not reachable from the source
}
