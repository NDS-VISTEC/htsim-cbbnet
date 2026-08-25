#!/bin/bash

real_network="clos_prio"

network="clos"
workload="mixed"
rate="40"

load_set=("5.00" "10.00" "15.00" "20.00" "25.00")
active="1.00"
seed_set=("1" "2" "3" "4" "5")

Ngroup_set=("18")
q="128"

for load in "${load_set[@]}"; do
    for seed in "${seed_set[@]}"; do
        for Ngroup in "${Ngroup_set[@]}"; do
            log_file="../../results/FCT_mixed/log_${real_network}_${Ngroup}Ngroup_${workload}_${load}pload_${q}q_seed=${seed}.txt"

            if [[ -s "$log_file" ]]; then
                echo "Log file $log_file already exists. Skipping..."
                continue
            fi

            echo "Processing $network $workload load $load, seed $seed"

            ../../src/clos/datacenter/htsim_ndp_fatTree_3to1_k12 \
                -cwnd 30 \
                -q "$q" \
                -strat perm \
                -nodes 648 \
                -simtime 10.00001 \
                -pullrate 1 \
                -link_rate_bps 40000000000 \
                -flowfile ../../traffic/mixed/$network/"$workload"_"$Ngroup"Ngroup_"$load"percLoad_10sec_72N_9hpr_648hosts_"$rate"Gbps_"$active"Nactive_seed="$seed".htsim \
                > "$log_file" &
        done
    done
done
