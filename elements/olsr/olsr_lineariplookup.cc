// -*- c-basic-offset: 4 -*-
/*
 * lineariplookup.{cc,hh} -- element looks up next-hop address in linear
 * routing table
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "olsr_lineariplookup.hh"
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/args.hh>
CLICK_DECLS

bool
cp_ip_route(String s, OLSRIPRoute *r_store, bool remove_route, Element *context)
{
    OLSRIPRoute r;
    if (!IPPrefixArg(true).parse(cp_shift_spacevec(s), r.addr, r.mask, context))
        return false;
    r.addr &= r.mask;
    String word = cp_shift_spacevec(s);
    if (word == "-")
    /* null gateway; do nothing */;
    else if (IPAddressArg().parse(word, r.gw, context))
    /* do nothing */;
    else
        goto two_words;

    word = cp_shift_spacevec(s);
    
two_words:
    
    if (IntArg().parse(word, r.port) /*|| (!word && remove_route)*/){
        
        if (!cp_shift_spacevec(s)) { // nothing left
            
            //Route is meant for last port which should be a discard port
            if (r.port == 255)
                r.port = context->noutputs()-1;
            
            *r_store = r;
            return true;
        }
    }
    
    return false;
}

String
status_willingness_string (String string_type, int value){
    String returnString = "";
    
    if (string_type == "status")
    {
        switch (value) {
            case 0:
                returnString = "Not_neighbor";
                break;
                
            case 1:
                returnString = "Sym_Neighbor";
                break;
                
            case 2:
                returnString = "MPR_Neighbor";
                break;
                
            default:
                returnString = "Invalid_value";
                break;
        }
        
    }else if (string_type == "willingness"){
        
        switch (value) {
            case 0:
                returnString = "Never";
                break;
                
            case 1:
                returnString = "Low";
                break;
                
            case 3:
                returnString = "Default";
                break;
                
            case 6:
                returnString = "High";
                break;
                
            case 7:
                returnString = "Always";
                break;
                
            default:
                returnString = "Invalid_value";
                break;
        }
    }
    
    return returnString;
}

double
metric_to_double(uint16_t etx_as_int)
{
    if (etx_as_int == 0)
        return 0;
    else
        return (((double)etx_as_int/100));
}

StringAccum&
OLSRIPRoute::unparse_with_hops(StringAccum& sa, bool tabs) const
{
    int l = sa.length();
    char tab = (tabs ? '\t' : ' ');
    String status_string = status_willingness_string("status", status);
    String willing_string = status_willingness_string("willingness", willingness);
    
    //sa << '\n';
    sa << addr.unparse_with_mask(mask) << tab;
    /*if (sa.length() < l + 17 && tabs)
        sa << '\t';*/
    l = sa.length();
    if (gw)
        sa << gw << tab;
    else
        sa << '-' << tab;
    /*if (sa.length() < l + 9 && tabs)
        sa << '\t';*/
    if (!real())
        sa << "-1";
    else{
        //sa << hops << tab << metric_to_double(etx) << tab << metric_to_double(variance) << tab << etx_time << tab << var_time << tab << metric_to_double(etx_min) << tab << etx_min_time << tab << metric_to_double(etx_max) << tab << etx_max_time << tab << status_string << tab << willing_string;
        sa << hops << tab << etx << tab << variance << tab << etx_time << tab << var_time << tab << etx_min << tab << etx_min_time << tab << etx_max << tab << etx_max_time << tab << status_string << tab << willing_string;
    }
    sa << '\n';
    return sa;
}

StringAccum&
OLSRIPRoute::unparse_without_hops(StringAccum& sa, bool tabs) const
{
    int l = sa.length();
    char tab = (tabs ? '\t' : ' ');
    String status_string = status_willingness_string("status", status);
    String willing_string = status_willingness_string("willingness", willingness);
    
    //sa << '\n';
    sa << addr.unparse_with_mask(mask) << tab;
    /*if (sa.length() < l + 17 && tabs)
        sa << '\t';*/
    l = sa.length();
    if (gw)
        sa << gw << tab;
    else
        sa << '-' << tab;
    /*if (sa.length() < l + 9 && tabs)
        sa << '\t';*/
    if (!real())
        sa << "-1";
    else
        sa << etx << tab << variance << tab << etx_time << tab << var_time << tab << etx_min << tab << etx_min_time << tab << etx_max << tab << etx_max_time << tab << status_string << tab << willing_string;
        //sa << metric_to_double(etx) << tab << metric_to_double(variance) << tab << etx_time << tab << var_time << tab << metric_to_double(etx_min) << tab << etx_min_time << tab << metric_to_double(etx_max) << tab << etx_max_time << tab << status_string << tab << willing_string;
    sa << '\n';
    return sa;
}

OLSRLinearIPLookup::OLSRLinearIPLookup()
: _zero_route(-1)
{
}

OLSRLinearIPLookup::~OLSRLinearIPLookup()
{
}

int
OLSRLinearIPLookup::initialize(ErrorHandler *)
{
    _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
    _last_addr2 = _last_addr;
#endif
    return 0;
}

int
OLSRLinearIPLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int r = 0, r1, eexist = 0;
    OLSRIPRoute route;
    for (int i = 0; i < conf.size(); i++) {
        if (!cp_ip_route(conf[i], &route, false, this)) {
            errh->error("argument %d should be %<ADDR/MASK [GATEWAY] OUTPUT%>", i+1);
            r = -EINVAL;
        } else if (route.port < 0 || route.port >= noutputs()) {
            errh->error("argument %d bad OUTPUT", i+1);
            r = -EINVAL;
        } else if ((r1 = add_route(route, false, 0, errh)) < 0) {
            if (r1 == -EEXIST)
                ++eexist;
            else
                r = r1;
        }
        
        if (i == 0) {
            if (r >= 0) {
                m_base_route = route;
                click_chatter("OLSRLinearIPLookup::configure - route set: \n%s", dump_routes().c_str());
            }else
                click_chatter("OLSRLinearIPLookup::configure - Error - base route not set");
        }
        
    }
    if (eexist)
        errh->warning("%d %s replaced by later versions", eexist, eexist > 1 ? "routes" : "route");
    return r;
}

String
OLSRLinearIPLookup::dump_routes()
{
    StringAccum sa;
    for (int i = 0; i < _t.size(); i++)
        if (_t[i].real())
            _t[i].unparse_with_hops(sa, true);
    return sa.take_string();
}

String
OLSRLinearIPLookup::dump_routes_without_hops()
{
    StringAccum sa;
    for (int i = 0; i < _t.size(); i++)
        if (_t[i].real())
            _t[i].unparse_without_hops(sa, true);
    return sa.take_string();
}

String
OLSRLinearIPLookup::dump_neighbors()
{
    StringAccum sa;
    for (int i = 0; i < _t.size(); i++){
        if (_t[i].hops < 2){
            if (_t[i].real())
                _t[i].unparse_without_hops(sa, true);
        }
    }
    
    //sa << "\n";
    return sa.take_string();
}

void
OLSRLinearIPLookup::push(int, Packet *p)
{
#define EXCHANGE(a,b,t) { t = a; a = b; b = t; }
    IPAddress a = p->dst_ip_anno();
    int ei = -1;
    
    if (a && a == _last_addr)
        ei = _last_entry;
#ifdef IP_RT_CACHE2
    else if (a && a == _last_addr2)
        ei = _last_entry2;
#endif
    else if ((ei = lookup_route_entry(a)) >= 0) {
#ifdef IP_RT_CACHE2
        _last_addr2 = _last_addr;
        _last_entry2 = _last_entry;
#endif
        _last_addr = a;
        _last_entry = ei;
    } else {
        static int complained = 0;
        if (++complained <= 5)
            click_chatter("OLSRLinearIPLookup: no route for %s", a.unparse().c_str());
        p->kill();
        return;
    }
    
    const OLSRIPRoute &e = _t[ei];
    if (e.gw){
        
        //This is due to the current Routing Tables are setup, when > 1 hop, the nexthop is correct, if 1 hop away, the next hop is the current node which is incorrect
        //NOTE: once you fix the RT's, there is no need for the 1 hop and else statement
        if (e.hops > 1){
            p->set_dst_ip_anno(e.gw);
            //click_chatter("OLSRLinearIPLookup::push - Dst: %s Nexthop %s Hops %d",a.unparse_mask().c_str(), e.gw.unparse_mask().c_str(), e.hops);
        }else{
            p->set_dst_ip_anno(a);
            //click_chatter("OLSRLinearIPLookup::push - Dst: %s Nexthop %s Hops %d",a.unparse_mask().c_str(), a.unparse_mask().c_str(), e.hops);
        }
        
    }
    
    //if (e.port > -1) {
        output(e.port).push(p);
    //}else{
        //Send to last port which be discard port
    //    output(noutputs()-1).push(p);
    //}
    
}

OLSRIPRoute
OLSRLinearIPLookup::get_base_route(void) const
{
    return m_base_route;
}

int
OLSRLinearIPLookup::run_command(int command, const String &str, Vector<OLSRIPRoute>* old_routes, ErrorHandler *errh)
{
    OLSRIPRoute route, old_route;
    if (!cp_ip_route(str, &route, command == CMD_REMOVE, this))
        return errh->error("expected %<ADDR/MASK [GATEWAY%s%>", (command == CMD_REMOVE ? " OUTPUT]" : "] OUTPUT"));
    else if (route.port < (command == CMD_REMOVE ? -1 : 0)
             || route.port >= noutputs())
        return errh->error("bad OUTPUT");
    
    int r, before = errh->nerrors();
    if (command == CMD_ADD){
        r = add_route(route, false, &old_route, errh);
        click_chatter("OLSRLinearIPLookup::run_command - added route - %s", str.c_str());
    }else if (command == CMD_SET){
        r = add_route(route, true, &old_route, errh);
        click_chatter("OLSRLinearIPLookup::run_command - set route - %s", str.c_str());
    }else
        r = remove_route(route, &old_route, errh);
    
    // save old route if in a transaction
    if (r >= 0 && old_routes) {
        if (old_route.port < 0) { // must come from add_route
            old_route = route;
            old_route.extra = CMD_ADD;
        } else
            old_route.extra = command;
        old_routes->push_back(old_route);
    }
    
    // report common errors
    if (r == -EEXIST && errh->nerrors() == before)
        errh->error("conflict with existing route %<%s%>", old_route.unparse().c_str());
    if (r == -ENOENT && errh->nerrors() == before)
        errh->error("route %<%s%> not found", route.unparse().c_str());
    if (r == -ENOMEM && errh->nerrors() == before)
        errh->error("no memory to store route %<%s%>", route.unparse().c_str());
    return r;
}

//Outputs a human-readable version of the current routing table.
String
OLSRLinearIPLookup::table_handler(Element *e, void *)
{
    String returnString;
    ErrorHandler* errh = 0;
    OLSRLinearIPLookup *q = static_cast<OLSRLinearIPLookup *>(e);
    click_chatter("OLSRLinearIPLookup::table_handler - requested routing table");
    OLSRIPRoute base_route = q->get_base_route();
    
    //Remove base so it's not printed
    q->remove_route(base_route, 0, errh);
    
    returnString = String(q->dump_routes());
    
    //Add back in base route
    q->add_route(base_route,false,0, errh);
    
    if (errh == 0)
        return returnString;
    else
        return "OLSRLinearIPLookup::table_handler() - Error removing/adding base route";
    
}

String
OLSRLinearIPLookup::read_neighbors_handler(Element *e, void *)
{
    OLSRLinearIPLookup *q = static_cast<OLSRLinearIPLookup *>(e);
    return String(q->dump_neighbors());
    
}


//"add" - Adds a route to the table. Format should be `ADDR/MASK [GW] OUT'. Fails if a route for ADDR/MASK already exists.
//"set" - Sets a route, whether or not a route for the same prefix already exists.
int
OLSRLinearIPLookup::add_route_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
    OLSRLinearIPLookup *table = static_cast<OLSRLinearIPLookup *>(e);
    return table->run_command((thunk ? CMD_SET : CMD_ADD), conf, 0, errh);
}

//"remove" - Removes a route from the table. Format should be `ADDR/MASK'.
int
OLSRLinearIPLookup::remove_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    click_chatter("OLSRLinearIPLookup::remove_route_handler - removed route - %s", conf.c_str());
    OLSRLinearIPLookup *table = static_cast<OLSRLinearIPLookup *>(e);
    return table->run_command(CMD_REMOVE, conf, 0, errh);
}

/*
int
OLSRLinearIPLookup::set_base_route(const String baseString, ErrorHandler *errh)
{
    int r = 0, r1, eexist = 0;
    OLSRIPRoute route;
    
    if (!cp_ip_route(baseString, &route, false, this)) {
        errh->error("argument should be %<ADDR/MASK [GATEWAY] OUTPUT%>");
        r = -EINVAL;
    } else if (route.port < 0 || route.port >= noutputs()) {
        errh->error("argument bad OUTPUT");
        r = -EINVAL;
    } else if ((r1 = add_route(route, false, 0, errh)) < 0) {
        if (r1 == -EEXIST)
            ++eexist;
        else
            r = r1;
    }
    
    if (r >= 0) {
        m_base_route = route;
        click_chatter("set_base_route - route set: \n%s", dump_routes().c_str());
    }else
        click_chatter("Error - base route not set");
    
    if (eexist)
        errh->warning("%d %s replaced by later versions", eexist, eexist > 1 ? "routes" : "route");
    return r;
}*/

int
OLSRLinearIPLookup::set_base_route_handler(const String &conf, Element *e, void */*thunk*/, ErrorHandler *errh)
{
    //int r = 0, r1, eexist = 0;
    //OLSRIPRoute route;
    Vector<String> confVector;
    confVector.push_back(conf);
    
    OLSRLinearIPLookup *table = static_cast<OLSRLinearIPLookup *>(e);
    return table->configure(confVector,errh);//set_base_route(conf,errh);
    
    
}

//Adds or removes a group of routes. Write `add/set ADDR/MASK [GW] OUT' to add a route, and `remove ADDR/MASK' to remove a route. You can supply multiple commands, one per line; all commands are executed as one atomic operation.
int
OLSRLinearIPLookup::ctrl_handler(const String &conf_in, Element *e, void *, ErrorHandler *errh)
{
    click_chatter("OLSRLinearIPLookup::ctrl_handler - %s", conf_in.c_str());
    OLSRLinearIPLookup *table = static_cast<OLSRLinearIPLookup *>(e);
    String conf = cp_uncomment(conf_in);
    const char* s = conf.begin(), *end = conf.end();
    
    Vector<OLSRIPRoute> old_routes;
    int r = 0;
    
    while (s < end) {
        const char* nl = find(s, end, '\n');
        String line = conf.substring(s, nl);
        
        String first_word = cp_shift_spacevec(line);
        int command;
        if (first_word == "add")
            command = CMD_ADD;
        else if (first_word == "remove")
            command = CMD_REMOVE;
        else if (first_word == "set")
            command = CMD_SET;
        else if (!first_word)
            continue;
        else {
            r = errh->error("bad command %<%#s%>", first_word.c_str());
            goto rollback;
        }
        
        if ((r = table->run_command(command, line, &old_routes, errh)) < 0)
            goto rollback;
        
        s = nl + 1;
    }
    return 0;
    
rollback:
    while (old_routes.size()) {
        const OLSRIPRoute& rt = old_routes.back();
        if (rt.extra == CMD_REMOVE)
            table->add_route(rt, false, 0, errh);
        else if (rt.extra == CMD_ADD)
            table->remove_route(rt, 0, errh);
        else
            table->add_route(rt, true, 0, errh);
        old_routes.pop_back();
    }
    return r;
}

//Reports the OUTput port and GW corresponding to an address.
int
OLSRLinearIPLookup
::lookup_handler(int, String& s, Element* e, const Handler*, ErrorHandler* errh)
{
    OLSRLinearIPLookup *table = static_cast<OLSRLinearIPLookup*>(e);
    IPAddress a;
    
    //click_chatter("OLSRLinearIPLookup::lookup_handler - Looked up - %s", s.c_str());
    
    if (IPAddressArg().parse(s, a, table)) {
        IPAddress gw;
        int port = table->lookup_route(a, gw);
        
        //Added 1 to port because NS3 IPAddress is located at "1", but Click Interface listed at "0". Havent checked this when >1 interfaces per router
        if (gw)
            s = String(port+1) + " " + gw.unparse();
        else
            s = String(port+1);
        
    
        //click_chatter("OLSRLinearIPLookup::lookup_handler - Returned - %s", s.c_str());
        return 0;
    } else
        return errh->error("expected IP address");
}

void
OLSRLinearIPLookup::add_handlers()
{
    add_write_handler("add", add_route_handler, 0);
    add_write_handler("set", add_route_handler, 1);
    add_write_handler("remove", remove_route_handler);
    add_write_handler("ctrl", ctrl_handler);
    add_write_handler("set_base_route", set_base_route_handler, 1);
    add_read_handler("table", table_handler, 0, Handler::f_expensive);
    set_handler("lookup", Handler::OP_READ | Handler::READ_PARAM, lookup_handler);
    add_read_handler("neighbors", read_neighbors_handler);
    //set_handler("set_rtable", Handler::OP_READ | Handler::READ_PARAM, set_table_handler);
}

bool
OLSRLinearIPLookup::check() const
{
    bool ok = true;
    //click_chatter("%s\n", ((LinearIPLookup*)this)->dump_routes().c_str());
    
    // 'next' pointers are correct
    for (int i = 0; i < _t.size(); i++) {
        if (!_t[i].real())
            continue;
        for (int j = i + 1; j < _t[i].extra && j < _t.size(); j++)
            if (_t[i].contains(_t[j]) && _t[j].real()) {
                click_chatter("%s: bad next pointers: routes %s, %s", declaration().c_str(), _t[i].unparse_addr().c_str(), _t[j].unparse_addr().c_str());
                ok = false;
            }
#if 0
        // This invariant actually does not hold.
        int j = _t[i].extra;
        if (j < _t.size())
            if (!_t[i].contains(_t[j]) && _t[j].real()) {
                click_chatter("%s: bad next pointers': routes %s, %s", declaration().c_str(), _t[i].unparse_addr().c_str(), _t[j].unparse_addr().c_str());
                ok = false;
            }
#endif
    }
    
    // no duplicate routes
    for (int i = 0; i < _t.size(); i++)
        for (int j = i + 1; j < _t.size(); j++)
            if (_t[i].addr == _t[j].addr && _t[i].mask == _t[j].mask && _t[i].real() && _t[j].real()) {
                //click_chatter("%s: duplicate routes for %s", declaration().c_str(), _t[i].unparse_addr().c_str());
                ok = false;
            }
    
    // caches point to the right place
    if (_last_addr && lookup_entry(_last_addr) != _last_entry) {
        click_chatter("%s: bad cache entry for %s", declaration().c_str(), _last_addr.unparse().c_str());
        ok = false;
    }
#ifdef IP_RT_CACHE2
    if (_last_addr2 && lookup_entry(_last_addr2) != _last_entry2) {
        click_chatter("%s: bad cache entry for %s", declaration().c_str(), _last_addr2.unparse().c_str());
        ok = false;
    }
#endif
    
    return ok;
}


int
OLSRLinearIPLookup::add_route(const OLSRIPRoute &r, bool allow_replace, OLSRIPRoute* replaced, ErrorHandler *, bool add_replica_routes)
{
    // overwrite any existing route
    int found = _t.size();

    if (!add_replica_routes){
        for (int i = 0; i < _t.size(); i++)
            if (!_t[i].real()) {
                if (found == _t.size())
                    found = i;
            } else if (_t[i].addr == r.addr && _t[i].mask == r.mask) {
                if (replaced)
                    *replaced = _t[i];
                if (!allow_replace)
                    return -EEXIST;
                _t[i].gw = r.gw;
                _t[i].port = r.port;
                _t[i].hops = r.hops;
                _t[i].etx = r.etx;
                _t[i].etx_time = r.etx_time;
                _t[i].variance = r.variance;
                _t[i].var_time = r.var_time;
                _t[i].etx_min = r.etx_min;
                _t[i].etx_min_time = r.etx_min_time;
                _t[i].etx_max = r.etx_max;
                _t[i].etx_max_time = r.etx_max_time;
                _t[i].status = r.status;
                _t[i].willingness = r.willingness;
                check();
                return 0;
            }
    }
    
    // maybe make a new slot
    if (found == _t.size())
        _t.push_back(r);
    else
        _t[found] = r;
    
    // patch up next pointers
    _t[found].extra = 0x7FFFFFFF;
    for (int i = found - 1; i >= 0; i--){
        if (_t[i].contains(r) && _t[i].extra > found)
            _t[i].extra = found;
    }
    
    for (int i = found + 1; i < _t.size(); i++){
        if (r.contains(_t[i]) && _t[i].real()) {
            _t[found].extra = i;
            break;
        }
    }
    
    // remember zero route
    if (!r.addr && r.mask.addr() == 0xFFFFFFFFU)
        _zero_route = found;
    
    // get rid of caches
    _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
    _last_addr2 = IPAddress();
#endif
    
    check();
    return 0;
}

int
OLSRLinearIPLookup::remove_route(const OLSRIPRoute& route, OLSRIPRoute* old_route, ErrorHandler *errh)
{
    for (int i = 0; i < _t.size(); i++)
        if (route.match(_t[i])) {
            if (old_route)
                *old_route = _t[i];
            _t[i].kill();
            
            // need to handle zero routes, bummer
            if (i == _zero_route)
                _zero_route = -1;
            else if (i < _zero_route) {
                OLSRIPRoute zero(_t[_zero_route]);
                _t[_zero_route].kill();
                int r = add_route(zero, false, 0, errh);
                assert(r >= 0);
                (void) r;
            }
            
            // get rid of caches
            _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
            _last_addr2 = IPAddress();
#endif
            check();
            return 0;
        }
    return -ENOENT;
}


/**
 * completely clears the routing table
 */
void
OLSRLinearIPLookup::clear()
{
     _t.clear();   		//clear table
    
    ErrorHandler* errh = 0;
    
    //Add back in base route
    add_route(m_base_route,false,0, errh);
    
    if (errh !=0)
        click_chatter("OLSRLinearIPLookup::clear() - ERROR adding base route back");
    //else
    //    click_chatter("Base route added back after clear. Have %d routes: \n%s", _t.size(), dump_routes().c_str());
        
    
     
    // get rid of caches
    _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
    _last_addr2 = IPAddress();
#endif
}

int
OLSRLinearIPLookup::lookup_entry(IPAddress a) const
{
    for (int i = 0; i < _t.size(); i++)
        if (_t[i].contains(a)) {
            int found = i;
            for (int j = _t[i].extra; j < _t.size(); j++)
                if (_t[j].contains(a) && _t[j].mask_as_specific(_t[found].mask))
                    found = j;
            return found;
        }
    return -1;
}

int
OLSRLinearIPLookup::lookup_entry_w_hops(IPAddress a, int h) const
{
    for (int i = 0; i < _t.size(); i++)
        if (_t[i].contains(a,h)) {
            int found = i;
            for (int j = _t[i].extra; j < _t.size(); j++)
                if (_t[j].contains(a,h) && _t[j].mask_as_specific(_t[found].mask))
                    found = j;
            return found;
        }
    return -1;
}

int
OLSRLinearIPLookup::lookup_entry_w_etx(IPAddress a, int e) const
{/*
    for (int i = 0; i < _t.size(); i++)
        if (_t[i].contains_better_metric(a,e)) {
            int found = i;
            for (int j = _t[i].extra; j < _t.size(); j++)
                if (_t[j].contains_better_metric(a,e) && _t[j].mask_as_specific(_t[found].mask))
                    found = j;
            return found;
        }
    return -1;*/
}

int
OLSRLinearIPLookup::lookup_route(IPAddress a, IPAddress &gw) const
{
    int ei = lookup_entry(a);
    if (ei >= 0) {
        gw = _t[ei].gw;
        return _t[ei].port;
    } else
        return -1;
}

int
OLSRLinearIPLookup::lookup_route_entry(IPAddress a) const
{
    for (int i = 0; i < _t.size(); i++)
        if (_t[i].contains(a)) {
            int found = i;
            for (int j = _t[i].extra; j < _t.size(); j++)
                if (_t[j].contains(a) && _t[j].mask_as_specific(_t[found].mask))
                    found = j;
            return found;
        }
    return -1;
}

int
OLSRLinearIPLookup::lookup_route_w_nexthop(IPAddress a, IPAddress g) const
{
    for (int i = 0; i < _t.size(); i++)
        if (_t[i].contains(a, g)) {
            int found = i;
            for (int j = _t[i].extra; j < _t.size(); j++)
                if (_t[j].contains(a,g) && _t[j].mask_as_specific(_t[found].mask))
                    found = j;
            return found;
        }
    return -1;
}

const OLSRIPRoute*
OLSRLinearIPLookup::lookup_iproute(const IPAddress& dst) const
{
    int index = lookup_route_entry(dst);
    if (index == -1) return 0;
    else return &_t[index];
}

const Vector<OLSRIPRoute*>
OLSRLinearIPLookup::lookup_iproute(const IPAddress& dst, int k_routes) //const
{
    //int index = -1;
    Vector<OLSRIPRoute*> routesFound;
    
    for (int i=0; i < k_routes; i++){
        int index = lookup_route_entry(dst);
        
        //if (index == -1)
        //    routesFound.push_back(0);
        //else
        if (index != -1)
            routesFound.push_back(&_t[index]);
        else
            break;
    }
    
    return routesFound;
    
}

const OLSRIPRoute*
OLSRLinearIPLookup::lookup_iproute_hop_count(const IPAddress& dst, int hop_count) const
{
    int index = lookup_entry_w_hops(dst, hop_count);
    if (index == -1) return 0;
    else return &_t[index];
}

const OLSRIPRoute*
OLSRLinearIPLookup::lookup_iproute_w_next_addr(const IPAddress& dst, const IPAddress& next_addr) const
{
    int index = lookup_route_w_nexthop(dst, next_addr);
    if (index == -1) return 0;
    else return &_t[index];
}

void 
//OLSRLinearIPLookup::update(const IPAddress& dst, const IPAddress& gw, int port, int extra, int hops, int etx, int variance, int status, int willingness) {
OLSRLinearIPLookup::update(const IPAddress& dst, const IPAddress& gw, int port, int extra, int hops, int etx, int variance, int status, int willingness) {
	int index = lookup_route_entry(dst);
	if (index == -1) return;
	
	_t[index].gw = gw;
	_t[index].port = port;
	_t[index].extra = extra;
    _t[index].hops = hops;
    _t[index].etx = etx;
    _t[index].variance = variance;
    _t[index].status = status;
    _t[index].willingness = willingness;
}

#include <click/vector.cc>
CLICK_ENDDECLS
ELEMENT_REQUIRES(LinearIPLookup)
EXPORT_ELEMENT(OLSRLinearIPLookup)
//ELEMENT_MT_SAFE(OLSRLinearIPLookup)
