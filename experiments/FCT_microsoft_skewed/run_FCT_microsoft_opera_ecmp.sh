#!/bin/bash

# for Opera, we set -cutoff 15 MB (specific to N=108 rack topology)
# we can also force flows of a certain size to use RLB using the -rlbflow flag (used in the shuffle experiment for 100 kB flows)


# Define a set of strings
real_network="opera_ecmp"

network="opera"
workload="HD"
rate="40"

load_set=("2.00" "4.00" "6.00" "8.00" "10.00")
active="1.00"
seed_set=("1" "2" "3" "4" "5")
gamma_set=("0.00" "0.25" "0.50" "0.75" "1.00")
# Iterate over the set of strings
for load in "${load_set[@]}"; do
    for seed in "${seed_set[@]}"; do
        for gamma in "${gamma_set[@]}"; do

            log_file="../../results/FCT_microsoft_skewed/log_"$real_network"_"$gamma"gamma_"$workload"_"$load"pload_seed="$seed".txt"

                if [[ -f "$log_file" ]]; then
                    echo "Log file $log_file already exists. Skipping..."
                    continue
                fi
            echo "Processing $network $workload load $load, active $active"

            ../../src/opera/datacenter/htsim_ndp_dynexpTopology \
                -cutoff 60000000 \
                -rlbflow 0 \
                -cwnd 20 \
                -q 32 \
                -simtime 10.00001 \
                -pullrate 1 \
                -link_rate_bps 40000000000 \
                -topfile ../../topologies/opera_108_ecmp.txt \
                -flowfile ../../traffic/microsoft_skewed/$network/"$workload"_"$gamma"gamma_"$load"percLoad_10sec_108N_6hpr_648hosts_"$rate"Gbps_"$active"Nactive_seed="$seed".htsim \
                > "$log_file" &
        done
    done
done      
