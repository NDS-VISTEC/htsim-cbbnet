#!/bin/bash

# for Opera, we set -cutoff 15 MB (specific to N=108 rack topology)
# we can also force flows of a certain size to use RLB using the -rlbflow flag (used in the shuffle experiment for 100 kB flows)


# Define a set of strings
real_network="clos_prio"

network="clos"
workload="DM"
rate="40"

load_set=("2.00" "4.00")
active="1.00"
seed_set=("1" "2" "3" "4" "5")

# Iterate over the set of strings
for load in "${load_set[@]}"; do
    for seed in "${seed_set[@]}"; do

        log_file="../../results/FCT_microsoft/log_"$real_network"_128q_"$workload"_"$load"pload_seed="$seed".txt"
            
        if [[ -f "$log_file" ]]; then
            echo "Log file $log_file already exists. Skipping..."
            continue
        fi

        echo "Processing $network $workload load $load, active $active"

        ../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 \
            -cwnd 30 \
            -q 128 \
            -strat perm \
            -nodes 648 \
            -simtime 10.00001 \
            -pullrate 1 \
            -link_rate_bps 40000000000 \
            -flowfile ../../traffic/microsoft/$network/"$workload"_"$load"percLoad_10sec_72N_9hpr_648hosts_"$rate"Gbps_"$active"Nactive_seed="$seed".htsim \
            > "$log_file" &
    done
done





