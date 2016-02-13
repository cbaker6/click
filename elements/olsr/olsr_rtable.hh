//TED 190404: Created

#ifndef OLSR_RTABLE_HH
#define OLSR_RTABLE_HH

#include <click/element.hh>
#include <click/bighashmap.hh>
#include "olsr_link_infobase.hh"
#include "olsr_neighbor_infobase.hh"
#include "olsr_topology_infobase.hh"
#include "olsr_interface_infobase.hh"
#include "olsr_association_infobase.hh"
#include "click_olsr.hh"
#include "olsr_lineariplookup.hh"

#define logging

CLICK_DECLS

class OLSRLinkInfoBase;
class OLSRNeighborInfoBase;
class OLSRTopologyInfoBase;
class OLSRInterfaceInfoBase;
class OLSRAssociationInfoBase;


class OLSRRoutingTable: public Element {
public:

    OLSRRoutingTable();
    ~OLSRRoutingTable();
    
    const char *class_name() const       { return "OLSRRoutingTable"; }
    OLSRRoutingTable *clone() const      { return new OLSRRoutingTable; }
    const char *port_count() const       { return "0/0"; }
    
    int configure(Vector<String> &conf, ErrorHandler *errh);
    void add_handlers() CLICK_COLD;
    int initialize(ErrorHandler *);
    void uninitialize();

    void print_routing_table(bool=false);
    void compute_routing_table(bool=false, Timestamp=Timestamp::make_msec(0,0));
    
protected:
    //Public methods for accessing private variables in add_handlers
    void clear_table(void);
    void set_route_locally(bool);
    void set_kroutes(int);
    int add_route_to_table(const OLSRIPRoute&, bool, OLSRIPRoute*, ErrorHandler *, bool=false);
    IPAddress get_address_from_interface(IPAddress);
    IPAddress get_my_address();
    int get_index_from_local_interface(IPAddress);
    static String print_table_handler(Element*, void *);
    static int set_table_handler(const String&, Element*, void *, ErrorHandler*);
    static int set_route_locally_handler(const String &conf, Element *e, void *, ErrorHandler * errh);
    static int set_kroutes_handler(const String &conf, Element *e, void *, ErrorHandler * errh);
    
private:
    
    //typedef HashMap<IPAddress, void *> RTable;
    //class RTable *_routingTable;
    bool _compute_routing_table_locally;
    IPAddress _myIP;
    IPAddress _myMask;
    int _kNumOfRoutes;
    OLSRLinkInfoBase *_linkInfo;
    OLSRNeighborInfoBase *_neighborInfo;
    OLSRInterfaceInfoBase *_interfaceInfo;
    OLSRTopologyInfoBase *_topologyInfo;
    OLSRLocalIfInfoBase *_localIfaces;
    OLSRAssociationInfoBase *_associationInfo;
    OLSRLinearIPLookup *_linearIPlookup;
    OLSRAssociationInfoBase *_visitorInfo;
    ErrorHandler *_errh;
    //static int set_table_handler(int operation, String&, Element*, const Handler*, ErrorHandler*);
    
    //  /bool _is_full_topology_table;

};


CLICK_ENDDECLS

#endif
