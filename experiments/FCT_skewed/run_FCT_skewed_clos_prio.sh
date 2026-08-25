#!/bin/bash

# for Opera, we set -cutoff 15 MB (specific to N=108 rack topology)
# we can also force flows of a certain size to use RLB using the -rlbflow flag (used in the shuffle experiment for 100 kB flows)


# Define a set of strings
real_network="clos_prio_20cwnd"

network="clos"
workload="HD"
rate="40"

load_set=("5.00")
active="1.00"
theta_set=("0.10" "0.20" "0.30" "0.40" "0.50")
phi_set=("0.50")
seed_set=("1" "2" "3" "4" "5")

# Iterate over the set of strings
for load in "${load_set[@]}"; do
    for theta in "${theta_set[@]}"; do
        for phi in "${phi_set[@]}"; do

            for seed in "${seed_set[@]}"; do

                echo "Processing $network $workload load $load, active $active"

                ../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 \
                    -cwnd 20 \
                    -q 128 \
                    -strat perm \
                    -nodes 648 \
                    -simtime 10.00001 \
                    -pullrate 1 \
                    -link_rate_bps 40000000000 \
                    -flowfile ../../traffic/skewed/$network/"$workload"_"$load"percLoad_10sec_72N_9hpr_648hosts_"$rate"Gbps_"$active"Nactive_"$theta"theta_"$phi"phi_seed="$seed".htsim \
                    > ../../results/FCT_skewed/log_"$real_network"_"$workload"_"$load"pload_"$theta"theta_"$phi"phi_seed="$seed".txt &
            done
        done
    done
done



