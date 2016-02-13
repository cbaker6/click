//TED 160404: Created
//Partially based on grid/linktable - implementation

#ifndef OLSR_LINK_INFOBASE_HH
#define OLSR_LINK_INFOBASE_HH 

#include <click/ipaddress.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include "olsr_neighbor_infobase.hh"
#include "olsr_interface_infobase.hh"
#include "olsr_duplicate_set.hh"
#include "olsr_tc_generator.hh"
#include "olsr_rtable.hh"
//#include "ippair.hh"
#include "../wifi/linktable.hh"
#include "click_olsr.hh"

CLICK_DECLS

class OLSRRoutingTable;
class OLSRNeighborInfoBase;
class OLSRInterfaceInfoBase;
class OLSRDuplicateSet;
class OLSRTCGenerator;

class OLSRLinkInfoBase: public Element{
public:

    OLSRLinkInfoBase();
    ~OLSRLinkInfoBase();
    
    const char* class_name() const { return "OLSRLinkInfoBase"; }
    OLSRLinkInfoBase *clone() const { return new OLSRLinkInfoBase(); }
    const char *port_count() const  { return "0/0"; }
    
    
    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *);
    void uninitialize();
    
    struct link_data *add_link(IPAddress local_addr, IPAddress neigh_addr, Timestamp time, Timestamp time_metric_enabled=Timestamp::make_msec(0,0), Timestamp time_next_hello_should_arrive=Timestamp::make_msec(0,0));
    struct link_data *find_link(IPAddress local_addr, IPAddress neigh_addr);
    bool update_link(IPAddress local_addr, IPAddress neigh_addr, Timestamp sym_time, Timestamp asym_time, Timestamp time, Timestamp time_metric_enabled=Timestamp::make_msec(0,0), int num_of_pkts_rec=-1, bool rec_enough_pkts_to_use_metric=false);
    void remove_link(IPAddress local_addr, IPAddress neigh_addr);
    HashMap<IPPair, void *> *get_link_set();
    void print_link_set();
    typedef HashMap<IPPair, void *> LinkSet;
    
private:
    
    LinkSet *_linkSet;
    
    OLSRNeighborInfoBase *_neighborInfo;
    OLSRInterfaceInfoBase *_interfaceInfo;
    OLSRRoutingTable *_routingTable;
    OLSRRoutingTable *_fullTopologyTable;
    OLSRDuplicateSet *_duplicateSet;
    OLSRTCGenerator *_tcGenerator;
    

    Timer _timer;
    
    
    void run_timer(Timer *);

};

CLICK_ENDDECLS
#endif
