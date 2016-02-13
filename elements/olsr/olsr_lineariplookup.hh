// -*- c-basic-offset: 4 -*-
#ifndef CLICK_OLSR_LINEARIPLOOKUP_HH
#define CLICK_OLSR_LINEARIPLOOKUP_HH
//#include "../ip/lineariplookup.hh"
#include "../ip/iproutetable.hh"

CLICK_DECLS

/*
=c

OLSRLinearIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s IP, classification

simple IP routing table

=d

B<Note:> Lookups and table updates with OLSRLinearIPLookup are extremely slow; the
RadixIPLookup and DirectIPLookup elements should be preferred in almost all
cases.  See IPRouteTable for a performance comparison.  We provide
OLSRLinearIPLookup nevertheless for its simplicity.

Expects a destination IP address annotation with each packet. Looks up that
address in its routing table, using longest-prefix-match, sets the destination
annotation to the corresponding GW (if specified), and emits the packet on the
indicated OUTput port.

Each argument is a route, specifying a destination and mask, an optional
gateway IP address, and an output port.

OLSRLinearIPLookup uses a linear search algorithm that may look at every route on
each packet. It is therefore most suitable for small routing tables.

=e

This example delivers broadcasts and packets addressed to the local
host (18.26.4.24) to itself, packets to net 18.26.4 to the
local interface, and all others via gateway 18.26.4.1:

  ... -> GetIPAddress(16) -> rt;
  rt :: OLSRLinearIPLookup(18.26.4.24/32 0,
                       18.26.4.255/32 0,
                       18.26.4.0/32 0,
                       18.26.4/24 1,
                       0/0 18.26.4.1 1);
  rt[0] -> ToHost;
  rt[1] -> ... -> ToDevice(eth0);

=h table read-only

Outputs a human-readable version of the current routing table.

=h lookup read-only

Reports the OUTput port and GW corresponding to an address.

=h add write-only

Adds a route to the table. Format should be `C<ADDR/MASK [GW] OUT>'.
Fails if a route for C<ADDR/MASK> already exists.

=h set write-only

Sets a route, whether or not a route for the same prefix already exists.

=h remove write-only

Removes a route from the table. Format should be `C<ADDR/MASK>'.

=h ctrl write-only

Adds or removes a group of routes. Write `C<add>/C<set ADDR/MASK [GW] OUT>' to
add a route, and `C<remove ADDR/MASK>' to remove a route. You can supply
multiple commands, one per line; all commands are executed as one atomic
operation.

=a RadixIPLookup, DirectIPLookup, RangeIPLookup, StaticIPLookup,
SortedIPLookup, LinuxIPLookup, IPRouteTable */

#define IP_RT_CACHE2 1

struct OLSRIPRoute : public IPRoute {
    
    int32_t status;
    int32_t willingness;
    int32_t hops;
    double etx;
    Timestamp etx_time;
    double variance;
    Timestamp var_time;
    double etx_min;
    Timestamp etx_min_time;
    double etx_max;
    Timestamp etx_max_time;
    
    OLSRIPRoute()   {IPRoute(); }
    OLSRIPRoute(IPAddress a, IPAddress m, IPAddress g, int p)
				:status(0), willingness(0), hops(0), etx(0), variance(0),
    etx_min(0), etx_max(0) { IPRoute(a,m,g,p);}
    
    inline bool contains(IPAddress a) const;
    inline bool contains(IPAddress a, IPAddress g) const;
    inline bool contains(IPAddress a, int h) const;
    inline bool contains(const OLSRIPRoute& r) const;
    inline bool contains(const OLSRIPRoute& r, int h) const;
    inline bool contains_better_metric(const OLSRIPRoute& r, int e) const;
    inline bool mask_as_specific(IPAddress m) const;
    inline bool mask_as_specific(const OLSRIPRoute& r) const;
    inline bool match(const OLSRIPRoute& r) const;
    
    StringAccum &unparse_with_hops(StringAccum&, bool tabs) const;
    StringAccum &unparse_without_hops(StringAccum&, bool tabs) const;

};

inline bool
OLSRIPRoute::contains(IPAddress a) const
{
    return (a.matches_prefix(addr, mask));
}

inline bool
OLSRIPRoute::contains(IPAddress a, IPAddress g) const
{
    return (a.matches_prefix(addr, mask) && g.matches_prefix(gw, mask));
}

inline bool
OLSRIPRoute::contains(IPAddress a, int h) const
{
    return (a.matches_prefix(addr, mask) && hops==h);
}

inline bool
OLSRIPRoute::contains(const OLSRIPRoute& r) const
{
    return r.addr.matches_prefix(addr, mask) && r.mask.mask_as_specific(mask);
}

inline bool
OLSRIPRoute::contains(const OLSRIPRoute& r, int h) const
{
    return r.addr.matches_prefix(addr, mask) && r.mask.mask_as_specific(mask) && r.hops==h;
}

inline bool
OLSRIPRoute::contains_better_metric(const OLSRIPRoute& r, int e) const
{
    return r.addr.matches_prefix(addr, mask) && r.mask.mask_as_specific(mask) && r.etx<e;
}

inline bool
OLSRIPRoute::mask_as_specific(IPAddress m) const
{
    return mask.mask_as_specific(m);
}

inline bool
OLSRIPRoute::mask_as_specific(const OLSRIPRoute& r) const
{
    return mask.mask_as_specific(r.mask);
}

inline bool
OLSRIPRoute::match(const OLSRIPRoute& r) const
{
    return addr == r.addr && mask == r.mask
    && (port < 0 || (gw == r.gw && port == r.port));
}

class OLSRLinearIPLookup : public /*LinearIPLookup*/IPRouteTable {

public:
    
    OLSRLinearIPLookup() CLICK_COLD;
    ~OLSRLinearIPLookup() CLICK_COLD;
    
    const char *class_name() const	{ return "OLSRLinearIPLookup"; }
    const char *port_count() const	{ return "1/-"; }
    const char *processing() const	{ return PUSH; }
    int configure(Vector<String>&, ErrorHandler*) CLICK_COLD;
    
    void add_handlers() CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void push(int port, Packet *p);
    
    const OLSRIPRoute * lookup_iproute(const IPAddress&) const;
    const Vector<OLSRIPRoute*> lookup_iproute(const IPAddress&, int); //const;
    const OLSRIPRoute * lookup_iproute_hop_count(const IPAddress&, int) const;
    const OLSRIPRoute * lookup_iproute_w_next_addr(const IPAddress&, const IPAddress&) const;

    String dump_routes();
    String dump_routes_without_hops();
    String dump_neighbors();
    
    int add_route(const OLSRIPRoute&, bool, OLSRIPRoute*, ErrorHandler *, bool=false);
    int remove_route(const OLSRIPRoute&, OLSRIPRoute*, ErrorHandler *);
    int lookup_route(IPAddress, IPAddress&) const;
    
    void clear();

    void update(const IPAddress& dst, const IPAddress& gw, int port, int extra, int hops = -1, int etx=-1, int variance=-1, int status=-1, int willingness=-1);
    
    typedef Vector<OLSRIPRoute>::iterator OLSRIPRouteTableIterator;
    OLSRIPRouteTableIterator begin() { return _t.begin(); };
    OLSRIPRouteTableIterator end() { return _t.end(); };
    OLSRIPRoute get_base_route(void) const;
    //int set_base_route(const String, ErrorHandler*);
    
    static int add_route_handler(const String&, Element*, void*, ErrorHandler*);
    static int set_base_route_handler(const String&, Element*, void*, ErrorHandler*);
    static int remove_route_handler(const String&, Element*, void*, ErrorHandler*);
    static int ctrl_handler(const String&, Element*, void*, ErrorHandler*);
    static int lookup_handler(int operation, String&, Element*, const Handler*, ErrorHandler*);
    static String table_handler(Element *, void * );
    static String read_neighbors_handler( Element *, void * );
    
    bool check() const;
    
protected:
    Vector<OLSRIPRoute> _t;
    int _zero_route;
    IPAddress _last_addr;
    int _last_entry;
    
#ifdef IP_RT_CACHE2
    IPAddress _last_addr2;
    int _last_entry2;
#endif
    
    int lookup_entry(IPAddress) const;
    int lookup_entry_w_hops(IPAddress, int) const;
    int lookup_entry_w_etx(IPAddress, int) const;
    int lookup_route_entry(IPAddress) const;
    int lookup_route_w_nexthop(IPAddress, IPAddress) const;
    
private:
    enum { CMD_ADD, CMD_SET, CMD_REMOVE, CMD_SET_TABLE };
    int run_command(int command, const String &, Vector<OLSRIPRoute>* old_routes, ErrorHandler*);
    OLSRIPRoute m_base_route; //A base route is needed for NS3 packets created at the kernal level
    
};

CLICK_ENDDECLS
//ELEMENT_REQUIRES(IPRouteTable)
//EXPORT_ELEMENT(OLSRLinearIPLookup)
#endif

