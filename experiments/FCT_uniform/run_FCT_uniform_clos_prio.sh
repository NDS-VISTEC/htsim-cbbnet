#!/bin/bash

# Define a set of strings
real_network="clos_prio_20cwnd"

network="clos"
workload="HD"
rate="40"

load_set=("2.00" "4.00" "6.00" "8.00" "10.00")
active="1.00"
seed_set=("1" "2" "3" "4" "5")
q="128"

# Iterate over the set of strings
for load in "${load_set[@]}"; do
    for seed in "${seed_set[@]}"; do

        # Construct the log file name
        log_file="../../results/FCT_uniform/log_${real_network}_${workload}_${load}pload_${q}q_seed=${seed}.txt"

        if [[ -f "$log_file" ]]; then
            echo "Log file $log_file already exists. Skipping..."
            continue
        fi

        echo "Processing $network $workload load $load, seed $seed"

        ../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 \
            -cwnd 20 \
            -q $q \
            -strat perm \
            -nodes 648 \
            -simtime 10.00001 \
            -pullrate 1 \
            -link_rate_bps 40000000000 \
            -flowfile ../../traffic/uniform/$network/"$workload"_"$load"percLoad_10sec_72N_9hpr_648hosts_"$rate"Gbps_"$active"Nactive_seed="$seed".htsim \
            > "$log_file" &

    done
done
