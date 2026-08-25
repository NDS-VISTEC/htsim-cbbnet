# CBB-Net

Packet-level simulation code for the IFIP Networking 2026 paper (CBB-Net) "CBB-Net: Towards a demand-aware periodic microsecond-switching RDCN for skewed traffic"

## Table of contents

1. [Description](#description)
2. [Build instructions](#build-instructions)
3. [Running the simulations and reproducing the results](#running-the-simulations-and-reproducing-the-results)
4. [Plotting the results](#plotting-the-results)
5. [Citation](#citation)


## Description

The `src/` directory contains the packet simulator source code.
There is a separate simulator for each network type (i.e., CBB-Net, Opera, Fat-tree).
The packet simulator is an extension of the htsim Opera simulator (https://github.com/TritonNetworking/opera-sim), itself derived from the htsim NDP simulator (https://github.com/nets-cs-pub-ro/NDP).

Periodic RDCNs such as Opera cyclically reconfigure their optical circuit switches (OCSes) so that the union of
matchings over one cycle emulates a *complete graph*, then load-balance large flows across it with Valiant Load
Balancing.
Real datacenter traffic is skewed, and fewer than half the top-of-rack switches (ToRs) are active at any instant,
so a complete graph wastes capacity: the load-balanced traffic of one flow bottlenecks the links another flow
needs.
CBB-Net instead emulates a *complete balanced bipartite (CBB)* graph, partitioning ToRs into an upper and a lower
half with edges only between halves, which forces two-hop forwarding.
When the active ToRs fit in one half, the idle ToRs act as relays and every flow reaches full throughput
efficiency.
A CBB graph over `N` ToRs also decomposes into `N/2` matchings instead of `N`, halving the cycle time, and any two
CBB graphs are isomorphic, so a new topology's matchings and routing labels are obtained by remapping the
existing ones instead of recomputing them.
Placing the active ToRs is solved as a Knapsack problem over connected components of the demand matrix; when no
feasible placement exists, CBB-Net falls back to emulating a complete graph.

Under highly skewed traffic, large flows achieve up to a 1.79x reduction in average FCT compared to Opera, while
small-flow performance is preserved.

### Repo Structure:
```
/
├─ topologies/ -- small topology inputs tracked in git, plus downloaded precomputed routing files
├─ src/ -- source for the htsim simulator
│  ├─ cbb/ -- CBB-Net, demand-aware periodic RDCN emulating CBB graphs
│  ├─ opera/ -- Opera(NSDI '20) varying expander, demand-oblivious periodic RDCN
│  ├─ clos/ -- 3:1 over-subscribed FatTree with priority queues
├─ experiments/ -- where simulator runs are initiated, grouped by workload and metric
├─ traffic/ -- where synthetic traffic traces are stored and can be generated
├─ plots/ -- FCT plotting notebooks and helpers
├─ results/ -- where simulator output is written
```

The CBB-Net-specific code lives in `src/cbb/datacenter/`:

* `mainSimulator.cpp` -- entry point, parses parameters, builds the network, loads flows and runs the event loop
* `CBB_Maker.*` -- CBB and all-to-all matchings, adjacency, routing labels, k-shortest paths and slice timing
* `CBB_Master.*` -- Knapsack active-ToR placement, connected components and isomorphic remapping
* `controller.*` -- central controller driving reconfiguration over the demand, calculation and broadcast phases
* `dynamicTopology.*` -- the reconfigurable topology over time
* `networkDevices.*` -- physical device model driven by the topology
* `tor.*` -- ToR switch behavior
* `main.h` -- compile-time topology constants, with alternate configurations as commented-out blocks

## Build instructions:

### Manual

To compile manually, from the root directory:

#### CBB-Net

```
cd src/cbb
make
cd datacenter
make
```

#### Opera

```
cd src/opera
make
cd datacenter
make
```

#### Clos

```
cd src/clos
make
cd datacenter
make
```

The executables will be built in the `datacenter/` subdirectories, named `htsim_mainSimulator` for CBB-Net,
`htsim_ndp_dynexpTopology` for Opera and `htsim_ndp_fatTree_3to1_k12` for Clos.

## Running the simulations and reproducing the results

### Fetching the missing topology files

The small CBB-Net topology input `topologies/DA_108.txt` is tracked in git. The larger Opera routing file and
CBB-Net precomputed routing-label file are provided separately to download
[HERE](https://drive.google.com/drive/folders/1zbqvyCeu7QPU1CHzfN7205w4ruJt76or?usp=share_link).
Download `opera_108_ecmp.txt` and `precomp_108.txt` into `topologies/` before running the full experiment scripts.
Traffic traces are not distributed as a full archive; regenerate the needed `*.htsim` files locally with the
provided traffic generators under `traffic/`.

For CBB-Net runs using the 108-ToR topology, `src/cbb/datacenter/CBB_Maker.cpp` looks for
`../../topologies/precomp_108.txt` relative to the experiment directory. This file stores precomputed adjacency
and routing-label data derived from `DA_108.txt`. If it is not present, the simulator attempts to regenerate it,
which can take a long time and requires enough memory, so downloading it with the other large topology artifacts
is recommended.

Traffic generator notebooks, helper code and flow-size distributions are kept in the repository. Generated
`*.htsim` trace files are ignored by git and should be regenerated locally, see [Generating traffic](#generating-traffic).

### Running the simulations via scripts

The most straight-forward way to reproduce a particular experiment is to run the scripts in the corresponding
directory under `experiments/`.
**Each script must be run from the directory it lives in**, as all paths inside it are relative:

```
cd experiments/FCT_microsoft
./run_FCT_microsoft_cbb.sh
```

Each script sweeps over a set of loads, seeds and demand-collection intervals defined at the top of the file, and
launches every combination in the background at once.
The script will also try finding existing complete simulation outputs before starting a certain simulation, so
that you do not repeat existing runs.
Output is written to `results/<experiment>/log_<system>_..._seed=<n>.txt`.

To reduce the number of concurrent runs, edit `load_set` and `seed_set` at the top of the script.

| Directory | Description |
|---|---|
| `FCT_microsoft/` | main FCT comparison under the Microsoft skewed traffic pattern |
| `FCT_microsoft_skewed/` | sweep of the skew parameter gamma |
| `FCT_uniform/`, `FCT_skewed/`, `FCT_mixed/` | other traffic patterns |
| `FCT_sensitivity/` | sensitivity to the demand-collection interval |
| `FCT_drop/` | robustness to demand-information packet loss |
| `FCT_hop_count/` | path length comparison |
| `Util_microsoft/` | link utilization over time |

### Generating traffic

Traffic traces are generated per network, because the host-to-ToR mapping differs: `108N_6hpr` for CBB-Net and
Opera, `72N_9hpr` for the Fat-tree, with 648 hosts in both cases.

Traces are regenerated with the per-workload notebooks under `traffic/` (e.g., `traffic/microsoft/gen_microsoft.ipynb`,
`traffic/uniform/gen_uniform.ipynb`), which call `get_flow_mat` from `traffic/utility.py` and draw flow sizes from
the CDFs in `traffic/_flow_dis/` (Datamining `DM`, Hadoop `HD`, `HD_10x` and Web Search `WS`).

Flow files are plain text with one flow per line:

`<src> <dst> <flowsize> <start_time_ns> [flow_id]`

The optional `flow_id` field is used when present. If it is omitted, the CBB-Net simulator uses the zero-based
line number as the flow id.

### Running the simulations manually

The executable for a particular network is `src/<network>/datacenter/<executable>`, where network is `cbb` for
CBB-Net, `opera` for Opera or `clos` for the FatTree network (e.g.,
`src/cbb/datacenter/htsim_mainSimulator` to run CBB-Net).

#### Parameters

There are numerous parameters to the simulator, some dependent on the network.

##### General parameters

* `-simtime s` run the simulation up to *s* seconds
* `-utiltime ms` sample link utilization every *ms* milliseconds
* `-topfile f` use topology file `f`
* `-flowfile f` run flows from flow file `f`
* `-q q` set maximum queue size for all queues to `q` MTU-size packets
* `-cwnd c` initial window `c` for transport
* `-cutoff b` treat flows of at least `b` bytes as large flows, served by RLB instead of NDP (e.g., 60000000)
* `-rlbflow b` force flows of exactly `b` bytes to use RLB
* `-link_rate_bps r` set link capacity to `r` bits per second (e.g., 40000000000)
* `-linkdelay d` set per-link propagation delay to `d` picoseconds
* `-pullrate p` sets rate of pulling to `p` of the maximum link bandwidth

##### CBB-Net parameters
* `-interval i` collect demand and recompute the topology every `i` superslices (e.g., 108, which is two cycles of the 54-matching CBB graph)
* `-delay_demand s` demand collection takes `s` superslices (default 2)
* `-delay_calculation s` topology calculation takes `s` superslices (default 4)
* `-delay_broadcast s` broadcasting the new topology takes `s` superslices (default 2)
* `-pdrop p` drop demand-information packets with probability `p` (e.g., 0.20)

#### Reading the output

##### FCT
Flow completion time of flows is reported as:

`FCT <src> <dst> <flowsize> <fct> <start_time>`

where `<fct>` and `<start_time>` are in microseconds.

##### Utilization
Link utilization is periodically reported when `-utiltime` is set.

##### Checking progress

To check how far a simulation is, you may want to execute the following bash command in the `results/` folder, and
substitute ``$SIMULATOR_OUTPUT`` with the output name to examine the simulation progress, when the script is still
running:

``grep FCT $SIMULATOR_OUTPUT | tail | awk '{print $5+$6;}'``

The output would be the time elapsed from the start of the simulation in microseconds.
Each simulation should finish when this output approaches the target end time.

## Plotting the results

The notebooks under `plots/FCT/` read the logs in `results/` and produce the FCT figures.
Flow completions are parsed by `get_data_from_file` in `plots/FCT/plot_utils.py`.

| Directory | Description |
|---|---|
| `plots/FCT/` | flow completion time comparisons across networks and traffic patterns |

Notebook outputs are stripped in the repository; re-run a notebook to regenerate its figures.
Generated figures and other local plotting work outside `plots/FCT/` are not tracked.

### Evaluation setup

The configuration used in the paper, as encoded in the scripts and topology files:

| | |
|---|---|
| Hosts | 648 |
| ToRs | 108 with 6 hosts each; 72 with 9 hosts each for the Fat-tree |
| OCSes | 6 |
| Link rate | 40 Gbps |
| Reconfiguration time | 10 us |
| Slice duration | 60.4 us |
| Cycle time | 3.8 ms for a CBB graph (54 matchings), 7.6 ms for a complete graph (108 matchings) |
| Demand collection | every two cycles (7.6 ms) |
| Large/small flow cutoff | 60 MB |
| Simulated duration | 10 s, repeated over 5 seeds |

## Citation

```bibtex
@inproceedings{rukpanich2026cbbnet,
  title     = {{CBB-Net}: Towards a demand-aware periodic microsecond-switching {RDCN} for skewed traffic},
  author    = {Rukpanich, Thatthep and Sintusiri, Teeruch and Phadungcharoen, Worames
               and Chantrapornchai, Chantana and Supittayapornpong, Sucha},
  booktitle = {IFIP Networking Conference},
  year      = {2026},
  isbn      = {978-3-903176-82-9},
}
```

This work was supported by the Thailand Science Research and Innovation (TSRI) under Grant No. FRB690039/0457.
