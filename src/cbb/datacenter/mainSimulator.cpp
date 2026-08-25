// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "config.h"
#include <sstream>
//#include <strstream>
#include <fstream> // needed to read flows
#include <iostream>
#include <string.h>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
#include "pipe.h"
#include "eventlist.h"
// #include "logfile.h"
// #include "loggers.h"
#include "clock.h"
#include "cbb_logging.h"
#include "ndp.h"
#include "compositequeue.h"
#include <list>
#include "main.h"

// Choose the topology here:
//#include "dynamicTopology.h"
#include "rlb.h"
#include "rlbmodule.h"
#include "networkDevices.h"

#include "tcp.h"
#include "controller.h"

#include <chrono>
// Simulation params

#define PRINT_PATHS 0

uint32_t delay_host2ToR = 0; // host-to-tor link delay in nanoseconds
uint32_t delay_ToR2ToR = 500; // tor-to-tor link delay in nanoseconds

#define DEFAULT_PACKET_SIZE 1500 // MAXIMUM FULL packet size (includes header + payload), Bytes
#define DEFAULT_HEADER_SIZE 64 // header size, Bytes
    // note: there is another parameter defined in `ndppacket.h`: "ACKSIZE". This should be set to the same size.
// set the NDP queue size in units of packets (of length DEFAULT_PACKET_SIZE Bytes)
#define DEFAULT_QUEUE_SIZE 8

string ntoa(double n); // convert a double to a string
string itoa(uint64_t n); // convert an int to a string

EventList eventlist;

Logfile* lg;

void exit_error(char* progr){
    cbb_logging::error() << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP" << endl;
    exit(1);
}


int main(int argc, char **argv) {

    // set maximum packet size in bytes:
    Packet::set_packet_size(DEFAULT_PACKET_SIZE - DEFAULT_HEADER_SIZE);
    mem_b queuesize = DEFAULT_QUEUE_SIZE * DEFAULT_PACKET_SIZE;

    // defined in flags:
    int cwnd = 30; // Default = 30
    //string queuename;
    string system;
    string flowfile; // read the flows from a specified file
    string topfile; // read the topology from a specified file
    double pull_rate = 1; // set the pull rate from the command line (default = 1)
    double simtime = 0; // seconds (default = 0)
    double utiltime = .01; // seconds
    int64_t rlbflow = 0; // flow size of "flagged" RLB flows
    int64_t cutoff = 0; // cutoff between NDP and RLB flow sizes. flows < cutoff == NDP.
    simtime_picosec link_delay = 0; // DEFAULT: LINK TO CONTROLLER IS 0
    int interval_delay = 0;


    int delay_demand = 2; // superslice
    int delay_calculation = 4; // superslice
    int delay_broadcast = 2; // superslice

    double pdrop = 0.0;
    bool verbose = false;
    int i = 1;
    RouteStrategy route_strategy = NOT_SET;
    
    // linkcap_UPDATE
    uint64_t link_rate_bps = 0;
    uint64_t link_rate = 10000000000;

    // parse the command line flags:
    while (i<argc) {
        if (!strcmp(argv[i],"-cwnd")){
	       cwnd = atoi(argv[i+1]);
	       i++;
        } else if (!strcmp(argv[i],"-q")){
	       queuesize = atoi(argv[i+1]) * DEFAULT_PACKET_SIZE;
	       i++;
        } else if (!strcmp(argv[i],"-cutoff")) {
            cutoff = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-rlbflow")) {
            rlbflow = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-flowfile")) {
			flowfile = argv[i+1];
			i++;
        } else if (!strcmp(argv[i],"-topfile")) {
            topfile = argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-pullrate")) {
            pull_rate = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-pdrop")) {
            pdrop = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-simtime")) {
            simtime = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-utiltime")) {
            utiltime = atof(argv[i+1]);
            i++;

        } else if (!strcmp(argv[i],"-linkdelay")) {
            link_delay = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-interval")) {
            interval_delay = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-link_rate_bps")) { // linkcap_UPDATE
            link_rate_bps = atof(argv[i+1]);
            link_rate = link_rate_bps/8;
            i++;
        } else if (!strcmp(argv[i],"-delay_demand")) {
            delay_demand = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-delay_calculation")) {
            delay_calculation = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-delay_broadcast")) {
            delay_broadcast = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-verbose")) {
            verbose = true;
        } else if (!strcmp(argv[i],"-quiet")) {
            verbose = false;
        } else {
            exit_error(argv[0]);
        }
        
        i++;
    }
    cbb_logging::set_verbose(verbose);

    eventlist.setEndtime(timeFromSec(simtime)); // in seconds
    Clock c(timeFromSec(5 / 100.), eventlist);

    route_strategy = SCATTER_RANDOM; // only one routing strategy for now...
      
    if (route_strategy == NOT_SET) {
	   fprintf(stderr, "Route Strategy not set.  Use the -strat param.  \nValid values are perm, rand, pull, rg and single\n");
	   exit(1);
    }

    if (link_rate_bps == 0) { // linkcap_UPDATE
	   fprintf(stderr, "set -link_rate_bps\n");
	   exit(1);
    }

    //cout << "cwnd " << cwnd << endl;
    //cout << "Logging to " << filename.str() << endl;
    // Logfile logfile(filename.str(), eventlist);

    NdpRtxTimerScanner ndpRtxScanner(timeFromMs(1), eventlist);

    delay_demand = delay_demand + delay_calculation + delay_broadcast;
    delay_calculation = delay_calculation + delay_broadcast;

// this creates the Expander topology
#ifdef DYNTOP
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Generating top..." << endl;
    dynamicTopology* top = new dynamicTopology(queuesize, eventlist, COMPOSITE, topfile, interval_delay, delay_calculation, delay_broadcast);
#endif
    
#ifdef NETDEV
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Generating net..." << endl;
    networkDevices* net = new networkDevices(queuesize, top, &eventlist, link_delay, link_rate, pdrop);
#endif  
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Connecting top and net..." << endl;
    top->netDev(net);

    Controller* CBB_controller = new Controller(net, top, eventlist, 0, interval_delay, delay_demand);
    CBB_controller->init();
    net->init_controller_sink(CBB_controller);
    top->set_controller(CBB_controller);
	// initialize all sources/sinks
    NdpSrc::setMinRTO(1000); // microseconds
    NdpSrc::setRouteStrategy(route_strategy);
    NdpSink::setRouteStrategy(route_strategy);

    // debug:
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Loading traffic..." << endl;

    //ifstream input("flows.txt");
    ifstream input(flowfile);
    if (!input.is_open()){
        cbb_logging::error() << "Unable to open flow file: " << flowfile << endl;
        return 1;
    }
    string line;
    int64_t temp;
    int flow_line = 0;
    // get flows. Format: (src) (dst) (bytes) (starttime nanoseconds) [(flow id)]
    while(getline(input, line)){
        flow_line++;
        vector<int64_t> vtemp;
        stringstream stream(line);
        while (stream >> temp)
            vtemp.push_back(temp);
        if (vtemp.empty())
            continue;
        if (vtemp.size() < 4){
            cbb_logging::error() << "Malformed flow record at " << flowfile << ":" << flow_line
                                 << " expected at least 4 integer fields" << endl;
            return 1;
        }
            // cout << "src = " << vtemp[0] << ", dest = " << vtemp[1] << ", bytes =  " << vtemp[2] << ", start_time[us] " << vtemp[3] << endl;

            // source and destination hosts for this flow
            int flow_src = vtemp[0];
            int flow_dst = vtemp[1];
            uint64_t flow_id = (vtemp.size() >= 5) ? vtemp[4] : flow_line - 1;
            
            
            if (vtemp[2] < cutoff && vtemp[2] != rlbflow) { // priority flow, sent it over NDP

                // generate an NDP source/sink:
                NdpSrc* flowSrc = new NdpSrc(net, top, NULL, NULL, eventlist, flow_src, flow_dst);
                flowSrc->setCwnd(cwnd*Packet::data_packet_size()); // congestion window
                flowSrc->set_flowsize(vtemp[2]); // bytes
                flowSrc->set_flowid(flow_id);
                // Set the pull rate to something reasonable.
                // we can be more aggressive if the load is lower
                NdpPullPacer* flowpacer = new NdpPullPacer(eventlist, pull_rate); // 1 = pull at line rate
                flowpacer->set_pull_rate(link_rate, pull_rate);
                
                //NdpPullPacer* flowpacer = new NdpPullPacer(eventlist, .17);

                NdpSink* flowSnk = new NdpSink(net, top, flowpacer, flow_src, flow_dst);
                ndpRtxScanner.registerNdp(*flowSrc);

                // set up the connection event
                flowSrc->connect(*flowSnk, timeFromNs(vtemp[3]/1.));

                //sinkLogger.monitorSink(flowSnk);

            }  else { // background flow, send it over RLB

                // generate an RLB source/sink:

                RlbSrc* flowSrc = new RlbSrc(net, top, NULL, NULL, eventlist, flow_src, flow_dst, cutoff);
                // debug:
                //cout << "setting flow size to " << vtemp[2] << " bytes..." << endl;
                flowSrc->set_flowsize(vtemp[2]); // bytes
                flowSrc->set_flowid(flow_id);
                RlbSink* flowSnk = new RlbSink(top, net, eventlist, flow_src, flow_dst);

                // set up the connection event
                flowSrc->connect(*flowSnk, timeFromNs(vtemp[3]/1.));

            }
    }

    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Traffic loaded." << endl;

    RlbMaster* master = new RlbMaster(net, top, eventlist, 1, interval_delay); // synchronizes the RLBmodules
    master->start();

    // NOTE: UtilMonitor defined in "pipe"
    UtilMonitor* UM = new UtilMonitor(top, net, eventlist);
    UM->start(timeFromSec(utiltime)); // print utilization every X milliseconds.
    
    if (cbb_logging::verbose_enabled()) cbb_logging::debug() << "Starting... " << endl;

    auto start_time = std::chrono::high_resolution_clock::now();
    while (eventlist.doNextEvent()) {
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(end_time - start_time);

    if (cbb_logging::verbose_enabled()) {
        cbb_logging::debug() << "Done" << endl;
        cbb_logging::debug() << "Time taken: " << duration.count()/60 << " hours " << duration.count()%60 << " minutes" << std::endl;
    }

}

string ntoa(double n) {
    stringstream s;
    s << n;
    return s.str();
}

string itoa(uint64_t n) {
    stringstream s;
    s << n;
    return s.str();
}
