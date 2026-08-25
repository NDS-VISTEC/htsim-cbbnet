#ifndef TOPOLOGY
#define TOPOLOGY
#include "network.h"

// interface class - set up functionality for derived topology classes

class Topology {
 public:
  virtual vector<const Route*>* get_paths(int src,int dest)=0;
  virtual vector<int>* get_neighbours(int src) = 0;  
  virtual int no_of_nodes() const { abort();};

  uint64_t get_link_rate() {return _link_rate;}
  void set_link_rate(uint64_t linkrate) {
    _link_rate = linkrate;
    cout << "linkrate (in topo) is set to " << this->_link_rate << " Bps (" << this->_link_rate*8 << " bps )" <<endl;
}
  private:
    uint64_t _link_rate;
};

#endif
