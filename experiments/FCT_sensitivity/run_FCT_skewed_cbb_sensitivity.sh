#!/bin/bash

# for Opera, we set -cutoff 15 MB (specific to N=108 rack topology)
# we can also force flows of a certain size to use RLB using the -rlbflow flag (used in the shuffle experiment for 100 kB flows)


# Define a set of strings
real_network="cbb"

network="opera"  ## same traffic as cbb
workload="DM"
rate="40"

active="1.00"
load_set=("5.00")
theta_set=("0.05" "0.10" "0.20" "0.50")
phi="0.50"
interval_set=("18" "36" "54" "216" "324")
seed_set=("1" "2" "3" "4" "5")

# Iterate over the set of strings
for load in "${load_set[@]}"; do
    for theta in "${theta_set[@]}"; do
        for seed in "${seed_set[@]}"; do
            for interval in "${interval_set[@]}"; do
                echo "Processing $real_network $workload load $load, active $active"

                ../../src/cbb/datacenter/htsim_mainSimulator \
                    -cutoff 60000000 \
                    -rlbflow 0 \
                    -cwnd 20 \
                    -q 32 \
                    -pdrop 0.00 \
                    -simtime 10.00001 \
                    -pullrate 1 \
                    -link_rate_bps 40000000000 \
                    -linkdelay 10000000 \
                    -interval $interval \
                    -topfile ../../topologies/DA_108.txt \
                    -flowfile ../../traffic/skewed/$network/"$workload"_"$load"percLoad_10sec_108N_6hpr_648hosts_"$rate"Gbps_"$active"Nactive_"$theta"theta_"$phi"phi_seed="$seed".htsim \
                    > ../../results/FCT_sensitivity/log_"$real_network"_"$interval"i_"$workload"_"$load"pload_"$theta"theta_"$phi"phi_seed="$seed".txt &
            done
        done
    done
done
