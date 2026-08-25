#include "networkDevices.h"
#include "rlbmodule.h"
#include "queue.h"
#include "pipe.h"
#include "compositequeue.h"
#include "main.h"
#include "controller.h"

extern uint32_t delay_host2ToR; // nanoseconds, host-to-tor link
extern uint32_t delay_ToR2ToR; // nanoseconds, tor-to-tor link

networkDevices::networkDevices(mem_b queuesize, dynamicTopology *top, EventList *eventlist, simtime_picosec delay, uint64_t link_rate, double pdrop)
: _top(top), link_delay(delay), _link_rate(link_rate) {
    _pdrop = pdrop;


    int nHpR = _top->no_of_hpr();
    int nNodes = _top->no_of_nodes();
    int nToRs = _top->no_of_tors();
    int nUpls = _top->no_of_upl();

    pipes_serv_tor.resize(nNodes);
    queues_serv_tor.resize(nNodes);
    rlb_modules.resize(nNodes);
    pipes_tor.resize(nToRs, vector<Pipe*>(nUpls + nHpR)); // Uplink + Downlink
    queues_tor.resize(nToRs, vector<Queue*>(nUpls + nHpR));
    top_of_rack_switch.resize(nToRs);

    for (int node = 0; node < nNodes; node++) {
        rlb_modules[node] = NULL;
        queues_serv_tor[node] = NULL;
        pipes_serv_tor[node] = NULL;
    }

    for (int node = 0; node < nNodes; node++) {
        rlb_modules[node] = new RlbModule(this, top, *eventlist, node);
        queues_serv_tor[node] = new PriorityQueue(this, top, speedFromMbps((uint64_t)_link_rate*8/1000000*nUpls/nHpR), memFromPkt(FEEDER_BUFFER), *eventlist, node);
        pipes_serv_tor[node] = new Pipe(timeFromNs(delay_host2ToR), *eventlist);
    }

    for (int j = 0; j < nToRs; j++){ // sweep ToR switches
        for (int k = 0; k < (nUpls+nHpR); k++) { // sweep ports
            queues_tor[j][k] = NULL;
            pipes_tor[j][k] = NULL;
        }
        top_of_rack_switch[j] = NULL;
    }

    for (int j = 0; j < nToRs; j++) { // sweep ToR switches

        top_of_rack_switch[j] = new ToR(this, top, j, nToRs, nUpls, nHpR);

        for (int k = 0; k < (nUpls+nHpR); k++) { // sweep ports
            if (k < nHpR) {
                top_of_rack_switch[j]->set_queue(k, new CompositeQueue(speedFromMbps((uint64_t)_link_rate*8/1000000*nUpls/nHpR), queuesize, *eventlist, j, k, _top));
                pipes_tor[j][k] = new Pipe(timeFromNs(delay_host2ToR), *eventlist);
            }
            else {
                // it's a link to another ToR
                top_of_rack_switch[j]->set_queue(k, new CompositeQueue(speedFromMbps((uint64_t)_link_rate*8/1000000), queuesize, *eventlist, j, k, _top));
                pipes_tor[j][k] = new Pipe(timeFromNs(delay_ToR2ToR), *eventlist);
            }
        }
    }
    // set rlb module to ToR
    for (int t = 0; t < nToRs; t++){
        for (int h = 0; h < nHpR; h++){
            top_of_rack_switch[t]->set_local_rlb_module(h, rlb_modules[t*nHpR + h]);
        }
    }

}

Queue* networkDevices::get_queue_tor(int tor, int port){
    return top_of_rack_switch[tor]->get_queue(port);
}

void networkDevices::init_controller_sink(Controller* controller){
    _conSink = new ControllerSink(controller, this, _pdrop);
    // add sink to ToR
    for (int t = 0; t < _top->no_of_tors(); t++){
        top_of_rack_switch[t]->set_controllerSink(_conSink);
    }
}
