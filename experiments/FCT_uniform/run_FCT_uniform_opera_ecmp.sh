#!/bin/bash

# Define a set of strings
real_network="opera_ecmp"

network="opera"
workload="HD"
rate="40"

# load_set=("2.00" "5.00" "10.00" "20.00")
load_set=("2.00" "4.00" "5.00" "10.00" "15.00")
active="1.00"
seed_set=("1" "2" "3" "4" "5")

# Iterate over the set of strings
for load in "${load_set[@]}"; do
    for seed in "${seed_set[@]}"; do

        # Construct the log file name
        log_file="../../results/FCT_uniform/log_${real_network}_${workload}_${load}pload_seed=${seed}.txt"

        if [[ -f "$log_file" ]]; then
            echo "Log file $log_file already exists. Skipping..."
            continue
        fi

        echo "Processing $network $workload load $load, seed $seed"

        ../../src/opera/datacenter/htsim_ndp_dynexpTopology \
            -cutoff 60000000 \
            -rlbflow 0 \
            -cwnd 20 \
            -q 32 \
            -simtime 10.00001 \
            -pullrate 1 \
            -link_rate_bps 40000000000 \
            -topfile ../../topologies/opera_108_ecmp.txt \
            -flowfile ../../traffic/uniform/$network/"$workload"_"$load"percLoad_10sec_108N_6hpr_648hosts_"$rate"Gbps_"$active"Nactive_seed="$seed".htsim \
            > "$log_file" &
    done
done
