//TED 150404: Created
/*
  =c
  Processes OLSR Hello messages, storing information in the OLSRNeighborInfoBase, OLSRLinkInfoBase, triggering MPR computation and Routing Table update if necessary
 
  =s
  OLSRProcessHello(OLSRLinkInfoBase element, OLSRNeighborInfoBase element, OLSRInterfaceInfoBase Element, OLSRRoutingTable Element, OLSRTCGenerator element, IPAddress)
 
  =io
  One input, one output
 
  =processing
  PUSH
 
  =d
  Gets OLSR Hello messages on input port. The incoming packets need to have their destionation address annotation set to the 1-hop source address of the message. Packets are parsed, and information is stored in the OLSRLinkInfoBase and OLSRNeighborInfoBase elements given as arguments. If the processing of the packet leads to the adding of an MPR Selector in the OLSRNeighborInfoBase element, the Advertise Neighbor Sequence Number (ANSN) in the OLSRTCGenerator element is updated. If a neighbor or 2-hop neighbor node is added in the OLSRNeighborInfoBase element, an MPR calculation is triggered in the OLSRNeighborInfobase element, and a routing table update is triggered in the OLSRRoutingTable element.
 
  =a
  OLSRProcessTC, OLSRProcessMID, OLSRClassifier, OLSRForward
 
*/
#ifndef OLSR_PROCESS_HELLO_HH
#define OLSR_PROCESS_HELLO_HH

#include <click/element.hh>
#include "olsr_link_infobase.hh"
#include "olsr_neighbor_infobase.hh"
#include "olsr_interface_infobase.hh"
#include "olsr_tc_generator.hh"
#include "olsr_rtable.hh"
#include "olsr_packethandle.hh"
#include "click_olsr.hh"
#include "olsr_local_if_infobase.hh"


CLICK_DECLS

/*
=c
OLSRProcessHello
 
=s
Processes OLSR Hello messages
 
=io
one input, one output
 
=d
Processes OSLR Hello messages, updating appropriate datastructures
 
=a
 
*/

class OLSRTCGenerator;
class OLSRRoutingTable;
class OLSRLinkInfoBase;
class OLSRNeighborInfoBase;
class OLSRInterfaceInfoBase;

class OLSRProcessHello : public Element
{
public:

    OLSRProcessHello();
    ~OLSRProcessHello();
    
    const char *class_name() const   { return "OLSRProcessHello"; }
    const char *processing() const   { return PUSH; }
    OLSRProcessHello *clone() const  { return new OLSRProcessHello(); }
    const char *port_count() const   { return "1/1"; }
    
    
    //Packet *simple_action(Packet *);
    
    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();
    //  int initialize(ErrorHandler *);
    //  void uninitialize();
    void push(int, Packet *);
    
    
    
protected:
    void set_period(int);
    void set_neighbor_hold_time_tv(int neighbor_hold_time);
    void set_window_size(int);
    static int set_neighbor_hold_time_tv_handler(const String &conf, Element *e, void *, ErrorHandler * errh);
    static int set_period_handler(const String &conf, Element *e, void *, ErrorHandler * errh);
    static int set_window_size_handler(const String &conf, Element *e, void *, ErrorHandler * errh);
    
private:
    
    void compute_htimes(void);
    
    
    int _lq_window_size;
    int _hello_period;
    int _neighbor_hold_time;
    
    Timestamp _neighbor_hold_time_tv;
    Timestamp _lq_window_time_tv;
    Timestamp _hello_time_tv;
    OLSRLinkInfoBase *_linkInfo;
    OLSRNeighborInfoBase *_neighborInfo;
    OLSRInterfaceInfoBase *_interfaceInfo;
    OLSRRoutingTable *_routingTable;
    OLSRRoutingTable *_fullTopologyTable;
    OLSRTCGenerator *_tcGenerator;
    OLSRLocalIfInfoBase *_localIfInfoBase;
    IPAddress _myMainIp;
};

CLICK_ENDDECLS
#endif
