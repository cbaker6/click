//TED 070504 :Created
/*
  =c
  OLSR specific element, generates OLSR TC messages
 
  =s
  OLSRTCGenerator(INTERVAL(msecs), OLSRNeighborInfoBase element, IPAddress)
  
  =io
  One output
 
  =processing
  PUSH
 
  =d
  Generates a TC message every INTERVAL msecs if the node has been selected as MPR by another node. The message is based on the info in the node's MPR Selector Set stored in the OLSRNeighborInfobase element given as argument.
 
  =a
  OLSRHelloGenerator, OLSRForward
  
*/
#ifndef OLSR_TC_GENERATOR_HH
#define OLSR_TC_GENERATOR_HH




#include <click/element.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include "olsr_neighbor_infobase.hh"
#include "click_olsr.hh"


CLICK_DECLS

class OLSRNeighborInfoBase;

class OLSRTCGenerator : public Element
{
public:

	OLSRTCGenerator();
	~OLSRTCGenerator();

	const char *class_name() const { return "OLSRTCGenerator"; }
	const char *processing() const { return PUSH; }
	OLSRTCGenerator *clone() const {return new OLSRTCGenerator(); }
  	const char *port_count() const { return "0/1"; }

	int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();
	int initialize(ErrorHandler *);

	Packet *generate_tc();
	Packet *generate_tc_when_not_mpr();
	void run_timer(Timer *);
	void set_node_is_mpr(bool value);
	void increment_ansn();
	void notify_mpr_selector_changed();
    
protected:
    void set_route_locally(bool);
    void set_period(int);
    void set_top_hold(int);
    static int set_route_locally_handler(const String &conf, Element *e, void *, ErrorHandler * errh);
    static int set_period_handler(const String &conf, Element *e, void *, ErrorHandler * errh);
    static int set_top_hold_handler(const String &conf, Element *e, void *, ErrorHandler * errh);
    
    
private:
    bool _compute_routing_table_locally;
	int _period;
	int _top_hold_time;
	bool _additional_TC_msg;
	uint8_t _vtime;
	uint16_t _ansn;
	Timestamp _end_of_validity_time;
	Timer _timer;
	OLSRNeighborInfoBase *_neighborInfo;
	IPAddress _myIP;
	bool _node_is_mpr;
	Timestamp _last_msg_sent_at;
	uint16_t get_ansn();
	uint8_t compute_vtime();
	bool _full_link_state;
	bool _mpr_full_link_state;
};

CLICK_ENDDECLS
#include <click/bighashmap.cc>
#endif

