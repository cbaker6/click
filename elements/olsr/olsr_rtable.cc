//TED 220404: Created

#include <click/config.h>
#include <click/error.hh>
#include <click/router.hh>
#include <click/confparse.hh>
#include <click/ipaddress.hh>
//#include "ippair.hh"
#include "../wifi/linktable.hh"
#include "olsr_rtable.hh"
#include "olsr_metric_etx.hh"
#include "olsr_metric_vtx.hh"
#include "olsr_metric_delta.hh"
#include "click_olsr.hh"
#include <string>
#include <click/args.hh>

// #define profiling

CLICK_DECLS

static String
getItemBeforeDelim(const String &str, const char delim, int i, int *delim_pos)
{
    const char *s = str.data() + i;
    const char *end = str.end();
    
    //StringAccum sa;
    const char *left = s;
    const char *right = s;
    
    while (s < end) {
        if (*s != delim){
            s++;
            right = s;
        }
        else
            break;
    }
    
    *delim_pos = s - str.begin();
    
    return str.substring(left, right);
    
}


Vector<String>
cp_parse_string_w_delim(const String &str, const char deliminator)
{
    Vector<String> returnString;
    
    int len = str.length();
    if (len == 0)
        return returnString;
    
    for (int pos = 0; pos < len; pos++) {
        String item = getItemBeforeDelim(str, deliminator, pos, &pos);
        if (item || pos < len)
            returnString.push_back(item);
    }
    
    return returnString;
}

OLSRRoutingTable::OLSRRoutingTable()
{
	_visitorInfo = 0;
}

OLSRRoutingTable::~OLSRRoutingTable()
{}


int
OLSRRoutingTable::configure( Vector<String> &conf, ErrorHandler *errh )
{
	if ( cp_va_kparse( conf, this, errh,
	                  "NEIGHBOR_INFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_neighborInfo,
	                  "LINK_INFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_linkInfo,
	                  "TOPOLOGY_INFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_topologyInfo,
	                  "INTERFACE_INFOBASE", cpkP+cpkM, cpElement, &_interfaceInfo,
	                  "LOCAL_INTERFACE_INFOBASE", cpkP+cpkM, cpElement, &_localIfaces,
	                  "ASSOCIATION_INFOBASE", cpkP+cpkM, cpElement, &_associationInfo,
	                  "ROUTING_TABLE", cpkP+cpkM, cpElement, &_linearIPlookup,
	                  "OWN_IPADDRESS", cpkP+cpkM, cpIPAddress, &_myIP,
                      "CREATE_ROUTES_LOCALLY", cpkP+cpkN, cpBool, &_compute_routing_table_locally,
                      "K_NUM_OF_ROUTES", cpkP+cpkN, cpInteger, &_kNumOfRoutes,
	                  "SUBNET_MASK", cpkN, cpIPAddress, &_myMask,
	                  "VISITOR_INFO", cpkN, cpElement, &_visitorInfo,
	                  cpEnd ) < 0 )
		return -1;

	_errh = errh;
	return 0;
}


int
OLSRRoutingTable::initialize( ErrorHandler *errh )
{
	if ( !_neighborInfo )
		return errh->error( "Could not find Neighbor InfoBase" );
	else if ( !_linkInfo )
		return errh->error( "Could not find Link InfoBase" );
	else if ( !_topologyInfo )
		return errh->error( "Could not find Topology InfoBase" );
	if ( !_interfaceInfo )
		return errh->error( "Could not find Interface InfoBase" );

	return 0;
}


void
OLSRRoutingTable::uninitialize()
{}


void
OLSRRoutingTable::print_routing_table(bool print_full_topology_table)
{
    
    //Disabled outing to the screen for faster simulation
    /*
    ErrorHandler* errh = 0;
    OLSRIPRoute base_route = _linearIPlookup->get_base_route();
    //Remove base so it's not printed
    _linearIPlookup->remove_route(base_route, 0, errh);
    
    
    Timestamp now = Timestamp::now();
     
    

	if (!print_full_topology_table)
		click_chatter( "ROUTING TABLE | Time: %f | MyIP: %s | \nDestination\tNext_hop\tHop_Cnt\tETX\tVar\tDelta_Time\tMetric_Time\tETX_Min\tETX_Min_Time\tETX_Max\tETX_Max_Time\tStatus\t\tWillingness\n%s\n", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), _linearIPlookup->dump_routes().c_str() );
	else
		click_chatter( "TOPOLOGY TABLE | Time: %f | MyIP: %s | \nDestination\tNext_hop\tETX\tVar\tDelta_Time\tMetric_time\tETX_Min\tETX_Min_Time\tETX_Max\tETX_Max_Time\tStatus\t\tWillingness\n%s\n", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), _linearIPlookup->dump_routes_without_hops().c_str() );
    
    //Add back in base route
    _linearIPlookup->add_route(base_route,false,0, errh);
    */
}


void
OLSRRoutingTable::compute_routing_table(bool compute_full_topology, Timestamp timeComputeTableWasCalled)
{
	HashMap<IPAddress, void *> *neighbor_set = _neighborInfo->get_neighbor_set();
	HashMap<IPPair, void *> *link_set = _linkInfo->get_link_set();
	HashMap<IPPair, void*> *twohop_set = _neighborInfo->get_twohop_set();
	HashMap<IPPair, void*> *topology_set = _topologyInfo->get_topology_set();
	HashMap<IPAddress, void*> *interface_set = _interfaceInfo->get_interface_set();
	HashMap<IPPair, void*> *association_set = _associationInfo->get_association_set();
	IPAddress netmask32( "255.255.255.255" );
	OLSRIPRoute newiproute;
	const OLSRIPRoute *iproute = 0;
    const OLSRIPRoute *iproute_to_test = 0;
    Vector<OLSRIPRoute*> iproutes_to_test;
    //bool compute_full_topology = false;
    
#ifdef profiling

	click_chatter ( "%f | %s | CALCULATING ROUTINGTABLE\n", Timestamp( now ).doubleval(), _myIP.unparse().c_str() );
	click_chatter ( "%f | %s | Neighbors: %d\ttwohopset: %d\ttopology_set: %d\tlink_set: %d\t interface_set %d\t\n", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), neighbor_set->size(), twohop_set->size(), topology_set->size(), link_set->size(), interface_set->size() );
	
	double time_dif;
    Timestamp now;
    Timestamp start = Timestamp::now();
#endif
	
    //if (!compute_full_topology){
	//_neighborInfo->print_neighbor_set();
	//_neighborInfo->print_mpr_set();
	//_neighborInfo->print_mpr_selector_set();
	//_neighborInfo->print_twohop_set();
	//_topologyInfo->print_topology();
    //}
    
    //If somebody else is computing routing tables for us, do nothing
    if (!_compute_routing_table_locally) {
        return;
    }
	
	//RFC ch 10 'Routing table calculation' - step 1 - delete all entries
	_linearIPlookup->clear();
    
#ifdef profiling

    now = Timestamp::now();
	time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
	click_chatter ( "%f | %s | time part 1 %f", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), time_dif );
    start = Timestamp::now();
#endif
    if (!compute_full_topology){
        //step 2 - adding routes to symmetric neighbors
        for ( HashMap<IPAddress, void*>::iterator iter = neighbor_set->begin(); iter != neighbor_set->end(); iter++ ) {
            neighbor_data *neighbor = ( neighbor_data * ) iter.value();
            if ( neighbor->N_status == OLSR_SYM_NEIGH ) {
                link_data * link = 0;
                link_data *lastlinktoneighbor = 0;
                bool neigh_main_addr_added = false;
                
                for ( HashMap<IPPair, void *>::iterator i = link_set->begin(); i != link_set->end(); i++ ) {
                    link = ( link_data * ) i.value();
                    IPAddress neigh_main_addr = _interfaceInfo->get_main_address( link->L_neigh_iface_addr );
                    if ( neigh_main_addr == neighbor->N_neigh_main_addr ) {
                        lastlinktoneighbor = link;
                        newiproute.addr = link->L_neigh_iface_addr;
                        newiproute.mask = netmask32;
                        newiproute.gw = link->L_neigh_iface_addr;
                        newiproute.port = _localIfaces->get_index( link->L_local_iface_addr );
                        newiproute.status = neighbor->N_status;
                        newiproute.willingness = neighbor->N_willingness;
                        newiproute.hops = 1;
#ifndef ENABLE_ETX
                        newiproute.etx = -1;
                        newiproute.variance = -1;
#else
                        newiproute.etx = link->etx;
                        newiproute.variance = link->variance;
                        newiproute.var_time = link->var_time;
                        newiproute.etx_min = link->etx_min;
                        newiproute.etx_min_time = link->etx_min_time;
                        newiproute.etx_max = link->etx_max;
                        newiproute.etx_max_time = link->etx_max_time;
                        if (newiproute.etx < OLSR_METRIC_INFINITY)
                            newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, link->etx_time);
                        else
                            newiproute.etx_time = Timestamp::make_msec(0,0);
                        
#endif
                        _linearIPlookup->add_route( newiproute, false, 0, _errh );
                        
                        if ( neigh_main_addr == link->L_neigh_iface_addr )
                            neigh_main_addr_added = true;
                        //	  click_chatter ("adding neighbor %s\n",link->L_neigh_iface_addr.unparse().c_str());
                    }
                }
                if ( ! neigh_main_addr_added && lastlinktoneighbor != 0 ) { //(lastlinktoneighbor != 0) should never fail
                    newiproute.addr = _interfaceInfo->get_main_address( lastlinktoneighbor->L_neigh_iface_addr );
                    newiproute.mask = netmask32;
                    newiproute.gw = lastlinktoneighbor->L_neigh_iface_addr;
                    newiproute.port = _localIfaces->get_index( lastlinktoneighbor->L_local_iface_addr );
                    newiproute.status = neighbor->N_status;
                    newiproute.willingness = neighbor->N_willingness;
                    newiproute.hops = 1;
#ifndef ENABLE_ETX
                    newiproute.etx = -1;
                    newiproute.variance = -1;
                    newiproute.etx_time = -1;
                    newiproute.var_time = -1;
                    newiproute.etx_min = -1;
                    newiproute.etx_min_time = -1;
                    newiproute.etx_max = -1;
                    newiproute.etx_max_time = -1;
#else
                    newiproute.etx = lastlinktoneighbor->etx;
                    newiproute.variance = lastlinktoneighbor->variance;
                    newiproute.var_time = lastlinktoneighbor->var_time;
                    newiproute.etx_min = lastlinktoneighbor->etx_min;
                    newiproute.etx_min_time = lastlinktoneighbor->etx_min_time;
                    newiproute.etx_max = lastlinktoneighbor->etx_max;
                    newiproute.etx_max_time = lastlinktoneighbor->etx_max_time;
                    
                    if (newiproute.etx < OLSR_METRIC_INFINITY)
                        newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, lastlinktoneighbor->etx_time);
                    else
                        newiproute.etx_time = Timestamp::make_msec(0,0);
                        
#endif
                    _linearIPlookup->add_route( newiproute, false, 0, _errh );
                }
            }
        }
#ifdef profiling
        now = Timestamp::now();
        time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
        click_chatter ( "%f | %s | time part 2 %f", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), time_dif );
        start = Timestamp::now();
#endif
        
        //step 3 - adding routes to twohop neighbors
        for ( HashMap<IPPair, void*>::iterator iter = twohop_set->begin(); iter != twohop_set->end(); iter++ ) {
            twohop_data *twohop = ( twohop_data * ) iter.value();
            if ( twohop->N_twohop_addr != _myIP ) {
#ifdef DEFAULT_OLSR
                
                
                //do not add twohop neighbors that have already been added
                if ( !_linearIPlookup->lookup_iproute( twohop->N_twohop_addr ) ) {
                    if ( (iproute = _linearIPlookup->lookup_iproute( twohop->N_neigh_main_addr )) ) {
                        neighbor_data *neighbor = ( neighbor_data * ) neighbor_set->find( twohop->N_neigh_main_addr );
                        if ( neighbor->N_willingness > OLSR_WILL_NEVER ) {
                            newiproute.addr = twohop->N_twohop_addr;
                            newiproute.mask = netmask32;
                            newiproute.gw = iproute->gw;
                            newiproute.port = iproute->port;
                            newiproute.status = twohop->N_status; //Currently, this is unknown, have to see if I want to pass quantity in tc messages
                            newiproute.willingness = twohop->N_willingness; //Assumed since two-hop neighbors are in tc messages
                            newiproute.hops = 2;
#ifndef ENABLE_ETX
                            newiproute.etx = -1;
                            newiproute.variance = -1;
                            newiproute.etx_time = -1;
                            newiproute.var_time = -1;
                            newiproute.etx_min = -1;
                            newiproute.etx_min_time = -1;
                            newiproute.etx_max = -1;
                            newiproute.etx_max_time = -1;
#else
                            newiproute.etx = twohop->etx;
                            newiproute.variance = twohop->variance;
                            if (newiproute.etx < OLSR_METRIC_INFINITY)
                                newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, twohop->etx_time, twohop->N_etx_delta_time);
                            else
                                newiproute.etx_time = Timestamp::make_msec(0,0);
                            
                            newiproute.var_time = twohop->var_time;
                            newiproute.etx_min = twohop->etx_min;
                            newiproute.etx_min_time = twohop->etx_min_time;
                            newiproute.etx_max = twohop->etx_max;
                            newiproute.etx_max_time = twohop->etx_max_time;
#endif
                            
                            _linearIPlookup->add_route( newiproute, false, 0, _errh );
                        }
                    }
                }
#elif defined ETX2
                //_kNumOfRoutes Can add up to two-hop neighbors with K
                iproutes_to_test = _linearIPlookup->lookup_iproute( twohop->N_twohop_addr,  _kNumOfRoutes);
                
                iproute_to_test = _linearIPlookup->lookup_iproute( twohop->N_twohop_addr );
                
                //Add route for all twohop neighbors where no routes currently existed
                if ( iproutes_to_test.size() < _kNumOfRoutes){
                //if ( !iproutes_to_test.at(0)){//!_linearIPlookup->lookup_iproute( twohop->N_twohop_addr ) ) {
                    
                    if ( (iproute = _linearIPlookup->lookup_iproute( twohop->N_neigh_main_addr )) ) {
                        neighbor_data *neighbor = ( neighbor_data * ) neighbor_set->find( twohop->N_neigh_main_addr );
                        if ( neighbor->N_willingness > OLSR_WILL_NEVER ) {
                            newiproute.addr = twohop->N_twohop_addr;
                            newiproute.mask = netmask32;
                            newiproute.gw = iproute->gw;
                            newiproute.port = iproute->port;
                            newiproute.status = twohop->N_status;
                            newiproute.willingness = twohop->N_willingness;
                            newiproute.hops = 2;
                            newiproute.etx = twohop->etx;
                            newiproute.variance = twohop->variance;
                            if (newiproute.etx < OLSR_METRIC_INFINITY)
                                newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, twohop->etx_time, twohop->N_etx_delta_time);
                            else
                                newiproute.etx_time = Timestamp::make_msec(0,0);
                            
                            newiproute.var_time = twohop->var_time;
                            newiproute.etx_min = twohop->etx_min;
                            newiproute.etx_min_time = twohop->etx_min_time;
                            newiproute.etx_max = twohop->etx_max;
                            newiproute.etx_max_time = twohop->etx_max_time;
                            _linearIPlookup->add_route( newiproute, false, 0, _errh );
                        }
                    }
                
                }else{
                    // If the neighbor is not new, check if there is a route with a better ETX value
                    neighbor_data *neighbor = ( neighbor_data * ) neighbor_set->find( twohop->N_neigh_main_addr );
                    if ( neighbor->N_willingness > OLSR_WILL_NEVER ) {
                
                        //For loop through k here
                        
                        //for ( Vector<OLSRIPRoute*>::iterator iter = iproutes_to_test.begin(); iter != iproutes_to_test.end(); iter++ ) {
                          //  iproute_to_test = &iter;
                        
                            if ( ((twohop->etx < iproute_to_test->etx) || (iproute_to_test->etx == 0) ) && (twohop->etx < OLSR_METRIC_INFINITY)){
                                
                                _linearIPlookup->remove_route(*iproute_to_test, 0, _errh );
                                
                                if ( (iproute = _linearIPlookup->lookup_iproute( twohop->N_neigh_main_addr )) ) {
                                    
                                    newiproute.addr = twohop->N_twohop_addr;
                                    newiproute.mask = netmask32;
                                    newiproute.gw = iproute->gw;
                                    newiproute.port = iproute->port;
                                    newiproute.status = twohop->N_status;
                                    newiproute.willingness = twohop->N_willingness;
                                    newiproute.hops = 2;
                                    
                                    newiproute.etx = twohop->etx;
                                    newiproute.variance = twohop->variance;
                                    if (newiproute.etx < OLSR_METRIC_INFINITY)
                                        newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, twohop->etx_time, twohop->N_etx_delta_time);
                                    else
                                        newiproute.etx_time = Timestamp::make_msec(0,0);
                                    
                                    newiproute.var_time = twohop->var_time;
                                    newiproute.etx_min = twohop->etx_min;
                                    newiproute.etx_min_time = twohop->etx_min_time;
                                    newiproute.etx_max = twohop->etx_max;
                                    newiproute.etx_max_time = twohop->etx_max_time;
                                    _linearIPlookup->add_route( newiproute, false, 0, _errh );
                                }
                            }
                        
                        //}
                    }
                }
#endif
            }
        }
#ifdef profiling
        
        now = Timestamp::now();
        time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
        click_chatter ( "%f | %s | time part 3 %f", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), time_dif );
        start = Timestamp::now();
#endif
        //step 4 - adding nodes with distance greater than 2
        for ( int h = 2; h >= 2; h++ ) {
            bool route_added = false;
            for ( HashMap<IPPair, void*>::iterator iter = topology_set->begin(); iter != topology_set->end(); iter++ ) {
                topology_data *topology = ( topology_data * ) iter.value();
                
#ifdef DEFAULT_OLSR
                if ( ! _linearIPlookup->lookup_iproute( topology->T_dest_addr ) ) {
                    //there is no entry in the routing table for this address
                    if ( (iproute = _linearIPlookup->lookup_iproute( topology->T_last_addr )) ) {
                        if ( iproute->hops == h ) {
                            // there is an entry for the last but one address in the route
                            newiproute.addr = topology->T_dest_addr;
                            newiproute.mask = netmask32;
                            newiproute.gw = topology->T_last_addr;
                            newiproute.port = iproute->port;
                            newiproute.status = topology->T_N_status; //Currently, this is unknown, have to see if I want to pass quantity in tc messages
                            newiproute.willingness = topology->T_N_willingness; //Assumed since two-hop neighbors are in tc messages
                            newiproute.hops = h + 1;
#ifndef ENABLE_ETX
                            newiproute.etx = -1;
                            newiproute.variance = -1;
                            newiproute.etx_time = -1;
                            newiproute.var_time = -1;
                            newiproute.etx_min = -1;
                            newiproute.etx_min_time = -1;
                            newiproute.etx_max = -1;
                            newiproute.etx_max_time = -1;
#else
                            newiproute.etx = OLSRMetricETX::sum_etx(iproute->etx, topology->T_N_etx);
                            
                            if ( newiproute.etx < OLSR_METRIC_INFINITY){
                                newiproute.variance = OLSRMetricVTX::calculate_covtx(1/newiproute.etx, 1);//OLSRMetricVTX::calculate_covtx(iproute->variance, topology->T_N_variance);
                                newiproute.var_time = topology->T_N_etx_time;//timeComputeTableWasCalled;
                                
                                newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, topology->T_N_etx_time, topology->T_N_etx_delta_time);
                                
                                
                                //Set ETX and variance of topology data
                                topology->T_etx = newiproute.etx;
                                topology->T_variance = newiproute.variance;
                                //topology->etx_time = newiproute.etx_time;
                                topology->var_time = newiproute.var_time;
                                
                                //Decide if the minimum and max metrics need to be updated
                                newiproute.etx_min = newiproute.etx;//newiproute.etx_min;
                                newiproute.etx_min_time = topology->T_N_etx_time;//timeComputeTableWasCalled;
                                topology->etx_min = newiproute.etx_min;
                                topology->etx_min_time = newiproute.etx_min_time;
                               
                                newiproute.etx_max = newiproute.etx;//newiproute.etx_max;
                                newiproute.etx_max_time = etx_max_time; //newiproute.etx_max_time;
                                topology->etx_max = newiproute.etx_max;
                                topology->etx_max_time = newiproute.etx_max_time;
                            }else{
                                newiproute.variance = OLSR_METRIC_INFINITY;
                                newiproute.var_time = Timestamp::make_msec(0,0);
                                newiproute.etx_min = OLSR_METRIC_INFINITY;
                                newiproute.etx_min_time = Timestamp::make_msec(0,0);
                                newiproute.etx_max = OLSR_METRIC_INFINITY;
                                newiproute.etx_max_time = Timestamp::make_msec(0,0);
                            }
                            
#endif
                            _linearIPlookup->add_route( newiproute, false, 0, _errh );
                            
                            route_added = true;
                        }
                    }
                }
#elif defined ETX2
                if ( ! _linearIPlookup->lookup_iproute( topology->T_dest_addr )){
                    
                    //Add any new topology neighbors. If the neighbor is not new, check if there is a route with a better ETX value
                    //there is no entry in the routing table for this address
                    if ( (iproute = _linearIPlookup->lookup_iproute( topology->T_last_addr )) ) {
                        if ( iproute->hops == h ) {
                            
                            int current_etx = OLSRMetricETX::sum_etx(iproute->etx, topology->T_N_etx);
                            int current_variance = OLSRMetricVTX::calculate_covtx(1/current_etx, 1);//OLSRMetricVTX::calculate_covtx(iproute->variance, topology->T_N_variance);
                            
                            // there is an entry for the last but one address in the route
                            newiproute.addr = topology->T_dest_addr;
                            newiproute.mask = netmask32;
                            newiproute.gw = iproute->gw;
                            newiproute.port = iproute->port;
                            newiproute.status = topology->T_N_status;
                            newiproute.willingness = topology->T_N_willingness;
                            newiproute.hops = h + 1;
                            
                            if (current_etx < OLSR_METRIC_INFINITY) {
                                
                                //Set ETX and variance for routes
                                newiproute.etx = current_etx;
                                newiproute.variance = current_variance;
                                newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, topology->T_N_etx_time, topology->T_N_etx_delta_time);
                                newiproute.var_time = timeComputeTableWasCalled;
                                
                                //Set ETX and variance of topology data
                                topology->T_etx = newiproute.etx;
                                topology->T_variance = newiproute.variance;
                                //topology->etx_time = newiproute.etx_time;
                                topology->var_time = newiproute.var_time;
                                
                                //Determine min and max values
                                if ((topology->etx_min < OLSR_METRIC_INFINITY) && (topology->etx_max < OLSR_METRIC_INFINITY)){
                                    if (newiproute.etx < topology->etx_min){
                                        newiproute.etx_min = newiproute.etx;
                                        newiproute.etx_min_time = topology->T_N_etx_time;//newiproute.etx_time;
                                        topology->etx_min = newiproute.etx_min;
                                        topology->etx_min_time = newiproute.etx_min_time;
                                    }else{
                                        newiproute.etx_min = topology->etx_min;
                                        newiproute.etx_min_time = topology->etx_min_time;
                                    }
                                    
                                    if (newiproute.etx > topology->etx_max){
                                        newiproute.etx_max = newiproute.etx;
                                        newiproute.etx_max_time = topology->T_N_etx_time;//newiproute.etx_time;
                                        topology->etx_max = newiproute.etx_max;
                                        topology->etx_max_time = newiproute.etx_max_time;
                                    }else{
                                        newiproute.etx_max = topology->etx_max;
                                        newiproute.etx_max_time = topology->etx_max_time;
                                    }
                                }else{
                                    newiproute.etx_min = newiproute.etx;
                                    newiproute.etx_min_time = topology->T_N_etx_time;//newiproute.etx_time;
                                    topology->etx_min = newiproute.etx_min;
                                    topology->etx_min_time = newiproute.etx_min_time;
                                    newiproute.etx_min = newiproute.etx;
                                    newiproute.etx_min_time = newiproute.etx_time;
                                    
                                    newiproute.etx_max = newiproute.etx;
                                    newiproute.etx_max_time = topology->T_N_etx_time;//newiproute.etx_time;
                                    topology->etx_max = newiproute.etx_max;
                                    topology->etx_max_time = newiproute.etx_max_time;
                                    newiproute.etx_max = newiproute.etx;
                                    newiproute.etx_max_time = newiproute.etx_time;
                                }
                                _linearIPlookup->add_route( newiproute, false, 0, _errh );
                                
                                route_added = true;
                            }
                        }
                    }
                    
                }else{
                    iproute_to_test = _linearIPlookup->lookup_iproute( topology->T_dest_addr );
                    
                    if ( (iproute = _linearIPlookup->lookup_iproute( topology->T_last_addr )) ) {
                        //there is entry in the routing table to the MPR of the destination node
                        if ( iproute->hops == h ) {
                            
                            int current_etx = OLSRMetricETX::sum_etx(iproute->etx, topology->T_N_etx);
                            int current_variance = OLSRMetricVTX::calculate_covtx(1/current_etx, 1);//OLSRMetricVTX::calculate_covtx(iproute->variance, topology->T_N_variance);
                            
                            //If new ETX value is real
                            //if ((topology->T_N_etx < OLSR_METRIC_INFINITY)){
                            if (current_etx < OLSR_METRIC_INFINITY){
                                //If there is a better ETX value or if the old route's ETX wasn't real, replace old route with new route. Note, lowest ETX with lowest hop count will be kept
                                if ( (current_etx < iproute_to_test->etx) || (iproute_to_test->etx == 0)){
                                    
                                    _linearIPlookup->remove_route(*iproute_to_test, 0, _errh );
                                    
                                    // there is an entry for the last but one address in the route
                                    newiproute.addr = topology->T_dest_addr;
                                    newiproute.mask = netmask32;
                                    newiproute.gw = iproute->gw;
                                    newiproute.port = iproute->port;
                                    newiproute.status = topology->T_N_status;
                                    newiproute.willingness = topology->T_N_willingness;
                                    newiproute.hops = h + 1;
                                    
                                    //newiproute.etx = topology->T_etx;
                                    //newiproute.variance = topology->T_variance;
                                    
                                    //Decide if the minimum and max metrics need to be updated
                                    //if (newiproute.etx < OLSR_METRIC_INFINITY){
                                    //  if (newiproute.etx < newiproute.etx){
                                    //newiproute.etx_min = topology->etx_min;
                                    //newiproute.etx_min_time = topology->etx_min_time;
                                    // }
                                    
                                    //if (newiproute.etx > newiproute.etx){
                                    //newiproute.etx_max = topology->etx_max;
                                    //newiproute.etx_max_time = topology->etx_max_time;
                                    // }
                                    //}
                                    //Set ETX and variance for routes
                                    newiproute.etx = current_etx;
                                    newiproute.variance = current_variance;
                                    newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, topology->T_N_etx_time, topology->T_N_etx_delta_time);
                                    newiproute.var_time = timeComputeTableWasCalled;
                                    
                                    //Set ETX and variance of topology data
                                    topology->T_etx = newiproute.etx;
                                    topology->T_variance = newiproute.variance;
                                    //topology->etx_time = newiproute.etx_time;
                                    topology->var_time = newiproute.var_time;
                                    
                                    //Determine min and max values
                                    if ((topology->etx_min < OLSR_METRIC_INFINITY) && (topology->etx_max < OLSR_METRIC_INFINITY)){
                                        if (newiproute.etx < topology->etx_min){
                                            newiproute.etx_min = newiproute.etx;
                                            newiproute.etx_min_time = topology->T_N_etx_time;//newiproute.etx_time;
                                            topology->etx_min = newiproute.etx_min;
                                            topology->etx_min_time = newiproute.etx_min_time;
                                        }else{
                                            newiproute.etx_min = topology->etx_min;
                                            newiproute.etx_min_time = topology->etx_min_time;
                                        }
                                        
                                        if (newiproute.etx > topology->etx_max){
                                            newiproute.etx_max = newiproute.etx;
                                            newiproute.etx_max_time = topology->T_N_etx_time;
                                            topology->etx_max = newiproute.etx_max;
                                            topology->etx_max_time = newiproute.etx_max_time;
                                        }else{
                                            newiproute.etx_max = topology->etx_max;
                                            newiproute.etx_max_time = topology->etx_max_time;
                                        }
                                    }else{
                                        newiproute.etx_min = newiproute.etx;
                                        newiproute.etx_min_time = topology->T_N_etx_time;//newiproute.etx_time;
                                        topology->etx_min = newiproute.etx_min;
                                        topology->etx_min_time = newiproute.etx_min_time;
                                        
                                        newiproute.etx_max = newiproute.etx;
                                        newiproute.etx_max_time = topology->T_N_etx_time;//newiproute.etx_time;
                                        topology->etx_max = newiproute.etx_max;
                                        topology->etx_max_time = newiproute.etx_max_time;
                                    }
                                    
                                    _linearIPlookup->add_route( newiproute, false, 0, _errh );
                                    
                                    route_added = true;
                                    
                                }
                            }
                        }
                    }
                    
                }
#endif
            }
            if ( ! route_added )
                break; //if no new nodes are added in an iteration, stop looking for new routes
        }
        
#ifdef profiling
        
        now = Timestamp::now();
        time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
        click_chatter ( "time part 4 %f", time_dif );
        start = Timestamp::now();
#endif
        
        //step 5 - add routes to other nodes' interfaces that have not already been added
        for ( HashMap<IPAddress, void *>::iterator iter = interface_set->begin(); iter != interface_set->end(); iter++ ) {
            interface_data *interface = ( interface_data * ) iter.value();
            //rtable_entry *entry = (rtable_entry *) _routingTable->find(interface->I_main_addr);
            if ( _linearIPlookup->lookup_iproute( interface->I_main_addr )  ) {
                if (! (iproute = _linearIPlookup->lookup_iproute( interface->I_iface_addr ) )) {
                    
                    newiproute.addr = interface->I_iface_addr;
                    newiproute.mask = netmask32;
                    newiproute.gw = iproute->gw;
                    newiproute.port = iproute->port;
                    newiproute.status = iproute->status;//-1;//neighbor->N_status; //Currently, this is unknown, have to see if I want to pass quantity in tc messages
                    newiproute.willingness = iproute->willingness;//OLSR_SYM_NEIGH;//neighbor->N_willingness; //Assumed since two-hop neighbors are in tc messages
                    newiproute.hops = iproute->hops;
#ifndef ENABLE_ETX
                    newiproute.etx = -1;
                    newiproute.variance = -1;
                    newiproute.etx_time = -1;
                    newiproute.var_time = -1;
                    newiproute.etx_min = -1;
                    newiproute.etx_min_time = -1;
                    newiproute.etx_max = -1;
                    newiproute.etx_max_time = -1;
#else
                    newiproute.etx = iproute->etx;
                    newiproute.variance = iproute->variance;
#endif
                    _linearIPlookup->add_route( newiproute, false, 0, _errh );
                }
            }
        }
        if ( _visitorInfo ) {
            _visitorInfo->clear();
            //	click_chatter ("%s | checking for new visitors!!!!\n",_myIP.unparse().c_str());
            for ( OLSRLinearIPLookup::OLSRIPRouteTableIterator iter = _linearIPlookup->begin() ; iter != _linearIPlookup->end(); iter++ ) {
                // click_chatter ("%s | CHECKING if node %s is visiting my network\n",_myIP.unparse().c_str(), _linearIPlookup->_t[index].addr.unparse().c_str());
                if ( iter->mask == netmask32 && !iter->addr.matches_prefix( _myIP, _myMask ) ) { // check if this node is on my subnet
                    _visitorInfo->add_tuple( iter->gw, iter->addr, iter->mask, Timestamp::make_msec( 0, 0 ) );
                    Timestamp now = Timestamp::now();
                    click_chatter ( "%f | %s | node %s is visiting my network\n", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), iter->addr.unparse().c_str() );
                }
            }
        }
        
        //step 6 - add routes to entries in the association table
        /// @TODO I don't feel that this code is 100% compliant, this definitely needs to be looked at and possibly rewritten (see p. 54 of the RFC)
        for ( HashMap<IPPair, void *>::iterator iter = association_set->begin(); iter != association_set->end(); iter++ ) {
            association_data *association = ( association_data * ) iter.value();
            if ( (iproute = _linearIPlookup->lookup_iproute( association->A_gateway_addr )) ) {
                if ( !_linearIPlookup->lookup_iproute( association->A_network_addr ) /** @TODO This seems like a weird or. have to check this again!! || _linearIPlookup->lookup_iproute( association->A_network_addr )->gw != iproute->gw */) {
                    newiproute.addr = association->A_network_addr;
                    newiproute.mask = association->A_netmask;
                    newiproute.gw = iproute->gw;
                    newiproute.port = iproute->port;
                    newiproute.status = iproute->status;//-1;//neighbor->N_status; //Currently, this is unknown, have to see if I want to pass quantity in tc messages
                    newiproute.willingness = iproute->willingness;//OLSR_SYM_NEIGH;//neighbor->N_willingness; //Assumed since two-hop neighbors are in tc messages
                    newiproute.hops = iproute->hops;
#ifndef ENABLE_ETX
                    newiproute.etx = -1;
                    newiproute.variance = -1;
                    newiproute.etx_time = -1;
                    newiproute.var_time = -1;
                    newiproute.etx_min = -1;
                    newiproute.etx_min_time = -1;
                    newiproute.etx_max = -1;
                    newiproute.etx_max_time = -1;
#else
                    newiproute.etx = iproute->etx;
                    newiproute.variance = iproute->variance;
#endif
                    _linearIPlookup->add_route( newiproute, false, 0, _errh );
                    
                } else {
                    if ( _linearIPlookup->lookup_iproute( association->A_network_addr )->hops > iproute->hops ) {
#ifndef ENABLE_ETX
                        _linearIPlookup->update(association->A_network_addr, iproute->gw, iproute->port, iproute->extra);
#else
                        _linearIPlookup->update(association->A_network_addr, iproute->gw, iproute->port, iproute->extra, iproute->hops, iproute->etx, iproute->variance);
#endif
                    }
                }
            }
        }
    }else{
        struct addr_w_route{
            IPAddress addr;
            OLSRIPRoute route;
        };
        //HashMap<IPAddress, OLSRIPRoute* > allFoundDst; //Used four keeping track of destinations
        Vector<addr_w_route> allFoundDst; //Used four keeping track of destinations
        HashMap<IPAddress, neighbor_data* > myOneHopNeighbors;
        //step 2 - adding routes to symmetric neighbors
        for ( HashMap<IPAddress, void*>::iterator iter = neighbor_set->begin(); iter != neighbor_set->end(); iter++ ) {
            neighbor_data *neighbor = ( neighbor_data * ) iter.value();
            if ( neighbor->N_status == OLSR_SYM_NEIGH ) {
                link_data * link = 0;
                link_data *lastlinktoneighbor = 0;
                bool neigh_main_addr_added = false;
                
                for ( HashMap<IPPair, void *>::iterator i = link_set->begin(); i != link_set->end(); i++ ) {
                    link = ( link_data * ) i.value();
                    IPAddress neigh_main_addr = _interfaceInfo->get_main_address( link->L_neigh_iface_addr );
                    if ( neigh_main_addr == neighbor->N_neigh_main_addr ) {
                        
                        //Store one hop neighbor address for later
                        myOneHopNeighbors.insert(link->L_neigh_iface_addr, neighbor);
                       
                        lastlinktoneighbor = link;
                        newiproute.addr = link->L_neigh_iface_addr;
                        newiproute.mask = netmask32;
                        newiproute.gw = _myIP;
                        newiproute.port = _localIfaces->get_index( link->L_local_iface_addr );
                        newiproute.status = neighbor->N_status;
                        newiproute.willingness = neighbor->N_willingness;
                        newiproute.hops = 1; //Note: These have to be 1 because of the topology table is currently calculated. All 1's are first hops of the respective node
#ifndef ENABLE_ETX
                        newiproute.etx = -1;
                        newiproute.variance = -1;
#else
                        newiproute.etx = link->etx;
                        newiproute.variance = link->variance;
                        if (newiproute.etx < OLSR_METRIC_INFINITY)
                            newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, link->etx_time);
                        else
                            newiproute.etx_time = Timestamp::make_msec(0,0);
                        
                        newiproute.var_time = link->var_time;
                        newiproute.etx_min = link->etx_min;
                        newiproute.etx_min_time = link->etx_min_time;
                        newiproute.etx_max = link->etx_max;
                        newiproute.etx_max_time = link->etx_max_time;
                        
#endif
                        addr_w_route addr_and_route;
                        addr_and_route.addr = newiproute.addr;
                        addr_and_route.route = newiproute;
                        allFoundDst.push_back(addr_and_route);
                        _linearIPlookup->add_route( newiproute, false, 0, _errh );
                        
                        if ( neigh_main_addr == link->L_neigh_iface_addr )
                            neigh_main_addr_added = true;
                        //	  click_chatter ("adding neighbor %s\n",link->L_neigh_iface_addr.unparse().c_str());
                    }
                }
                if ( ! neigh_main_addr_added && lastlinktoneighbor != 0 ) { //(lastlinktoneighbor != 0) should never fail
                    newiproute.addr = _interfaceInfo->get_main_address( lastlinktoneighbor->L_neigh_iface_addr );
                    //Store one hop neighbor address for later
                    myOneHopNeighbors.insert(newiproute.addr, neighbor);
                    newiproute.mask = netmask32;
                    newiproute.gw = _myIP;
                    newiproute.port = _localIfaces->get_index( lastlinktoneighbor->L_local_iface_addr );
                    newiproute.status = neighbor->N_status;
                    newiproute.willingness = neighbor->N_willingness;
                    newiproute.hops = 1;
#ifndef ENABLE_ETX
                    newiproute.etx = -1;
                    newiproute.variance = -1;
                    newiproute.etx_time = -1;
                    newiproute.var_time = -1;
                    newiproute.etx_min = -1;
                    newiproute.etx_min_time = -1;
                    newiproute.etx_max = -1;
                    newiproute.etx_max_time = -1;
#else
                    newiproute.etx = lastlinktoneighbor->etx;
                    newiproute.variance = lastlinktoneighbor->variance;
                    if (newiproute.etx < OLSR_METRIC_INFINITY)
                        newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, lastlinktoneighbor->etx_time);
                    else
                        newiproute.etx_time = Timestamp::make_msec(0,0);
                   
                    newiproute.var_time = lastlinktoneighbor->var_time;
                    newiproute.etx_min = lastlinktoneighbor->etx_min;
                    newiproute.etx_min_time = lastlinktoneighbor->etx_min_time;
                    newiproute.etx_max = lastlinktoneighbor->etx_max;
                    newiproute.etx_max_time = lastlinktoneighbor->etx_max_time;
#endif
                    addr_w_route addr_and_route;
                    addr_and_route.addr = newiproute.addr;
                    addr_and_route.route = newiproute;
                    allFoundDst.push_back(addr_and_route);
                    _linearIPlookup->add_route( newiproute, false, 0, _errh );
                }
            }
        }
#ifdef profiling
        now = Timestamp::now();
        time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
        click_chatter ( "%f | %s | time part 2 %f", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), time_dif );
        start = Timestamp::now();
#endif
        
        for (HashMap<IPAddress, neighbor_data*>::iterator iterOneHop = myOneHopNeighbors.begin(); iterOneHop != myOneHopNeighbors.end(); iterOneHop++) {
            //IPAddress neigh_main_addr = _interfaceInfo->get_main_address(it.value());
            
            neighbor_data *neighbor = iterOneHop.value();
            //Add route from me to neighbor
            newiproute.addr = _myIP;
            newiproute.mask = netmask32;
            newiproute.gw = iterOneHop.key();
            //newiproute.port = iproute->port;
            newiproute.status = neighbor->N_status; //Currently, this is unknown, have to see if I want to pass quantity in tc messages
            newiproute.willingness = neighbor->N_willingness; //Assumed since two-hop neighbors are in tc messages
            newiproute.hops = 2; //Note: These have to be  > 1 because of the topology table is currently calculated
#ifndef ENABLE_ETX
            newiproute.etx = -1;
            newiproute.variance = -1;
            newiproute.etx_time = -1;
            newiproute.var_time = -1;
            newiproute.etx_min = -1;
            newiproute.etx_min_time = -1;
            newiproute.etx_max = -1;
            newiproute.etx_max_time = -1;
#else
            newiproute.etx = neighbor->etx;
            newiproute.variance = neighbor->variance;
            if (newiproute.etx < OLSR_METRIC_INFINITY)
                newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, neighbor->etx_time);
            else
                newiproute.etx_time = Timestamp::make_msec(0,0);
            
            newiproute.var_time = neighbor->var_time;
            newiproute.etx_min = neighbor->etx_min;
            newiproute.etx_min_time = neighbor->etx_min_time;
            newiproute.etx_max = neighbor->etx_max;
            newiproute.etx_max_time = neighbor->etx_max_time;
#endif
            _linearIPlookup->add_route( newiproute, false, 0, _errh, true );
            
            //Vector<IPAddress> twohop_address;
            //step 3 - adding routes to twohop neighbors
            for ( HashMap<IPPair, void*>::iterator iter = twohop_set->begin(); iter != twohop_set->end(); iter++ ) {
                twohop_data *twohop = ( twohop_data * ) iter.value();
                if ( twohop->N_neigh_main_addr == iterOneHop.key() ) {
                    if (twohop->N_twohop_addr != _myIP){
                        //if ( (iproute = _linearIPlookup->lookup_iproute_hop_count( twohop->N_neigh_main_addr, 1 )) ) {
                        //neighbor_data *neighbor = ( neighbor_data * ) neighbor_set->find( twohop->N_neigh_main_addr );
                        //Add route
                        newiproute.addr = twohop->N_twohop_addr;
                        newiproute.mask = netmask32;
                        newiproute.gw = twohop->N_neigh_main_addr;//iproute->addr;//iproute->gw;
                        //newiproute.port = iproute->port;
                        newiproute.status = twohop->N_status; //Currently, this is unknown, have to see if I want to pass quantity in tc messages
                        newiproute.willingness = twohop->N_willingness; //Assumed since two-hop neighbors are in tc messages
                        newiproute.hops = 2;
#ifndef ENABLE_ETX
                        newiproute.etx = -1;
                        newiproute.variance = -1;
                        newiproute.etx_time = -1;
                        newiproute.var_time = -1;
                        newiproute.etx_min = -1;
                        newiproute.etx_min_time = -1;
                        newiproute.etx_max = -1;
                        newiproute.etx_max_time = -1;
#else
                        newiproute.etx = twohop->N_etx;
                        if (newiproute.etx < OLSR_METRIC_INFINITY)
                            newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, twohop->N_etx_time, twohop->N_etx_delta_time);
                        else
                            newiproute.etx_time = Timestamp::make_msec(0,0);
                        
                        newiproute.variance = twohop->N_variance;
                        newiproute.var_time = twohop->N_var_time;
                        newiproute.etx_min = twohop->N_etx_min;
                        newiproute.etx_min_time = twohop->N_etx_min_time;
                        newiproute.etx_max = twohop->N_etx_max;
                        newiproute.etx_max_time = twohop->N_etx_max_time;
#endif
                        addr_w_route addr_and_route;
                        addr_and_route.addr = newiproute.addr;
                        addr_and_route.route = newiproute;
                        allFoundDst.push_back(addr_and_route);
                        _linearIPlookup->add_route( newiproute, false, 0, _errh, true );
                        
                        //}
                    }
                }
            }
        }
        
#ifdef profiling
        
        now = Timestamp::now();
        time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
        click_chatter ( "%f | %s | time part 3 %f", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), time_dif );
        start = Timestamp::now();
#endif
        for ( HashMap<IPPair, void*>::iterator iter = topology_set->begin(); iter != topology_set->end(); iter++ ) {
            topology_data *topology = ( topology_data * ) iter.value();
            
            //If this route isn't currently in the topology table
            if ( (! _linearIPlookup->lookup_iproute_w_next_addr(topology->T_dest_addr, topology->T_last_addr )) ) {
                
                newiproute.addr = topology->T_dest_addr;
                newiproute.mask = netmask32;
                newiproute.gw = topology->T_last_addr;
                //newiproute.port = iproute->port;
                newiproute.status = topology->T_N_status;
                newiproute.willingness = topology->T_N_willingness;
                newiproute.hops = 3;
#ifndef ENABLE_ETX
                newiproute.etx = -1;
                newiproute.variance = -1;
                newiproute.etx_time = -1;
                newiproute.var_time = -1;
                newiproute.etx_min = -1;
                newiproute.etx_min_time = -1;
                newiproute.etx_max = -1;
                newiproute.etx_max_time = -1;
#else
                newiproute.etx = topology->T_N_etx;//OLSRMetricETX::sum_etx(iproute->etx, topology->T_N_etx);
                if (newiproute.etx < OLSR_METRIC_INFINITY)
                    newiproute.etx_time = OLSRMetricDelta::calculate_delta(timeComputeTableWasCalled, topology->T_N_etx_time, topology->T_N_etx_delta_time);
                else
                    newiproute.etx_time = Timestamp::make_msec(0,0);
                
                newiproute.variance = topology->T_N_variance;//OLSRMetricVTX::calculate_covtx(iproute->variance, topology->T_N_variance);
                newiproute.var_time = topology->T_N_var_time;
                newiproute.etx_min = topology->T_N_etx_min;//newiproute.etx_min;
                newiproute.etx_min_time = topology->T_N_etx_min_time;//timeComputeTableWasCalled;
                newiproute.etx_max = topology->T_N_etx_max;//newiproute.etx_max;
                newiproute.etx_max_time = topology->T_N_etx_max_time;//newiproute.etx_max_time;
#endif
                addr_w_route addr_and_route;
                addr_and_route.addr = newiproute.addr;
                addr_and_route.route = newiproute;
                allFoundDst.push_back(addr_and_route);
                _linearIPlookup->add_route( newiproute, false, 0, _errh, true );
                
            }
        }
        
#ifdef profiling
        
        now = Timestamp::now();
        time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
        click_chatter ( "%f | %s | time part 3 %f", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), time_dif );
        start = Timestamp::now();
#endif
        //Add routes that can be inferred from the current table, but have not received tc messages for
        for ( Vector<addr_w_route>::iterator iter = allFoundDst.begin(); iter != allFoundDst.end(); iter++ ) {
            OLSRIPRoute *current_route = &iter->route;
            
            if (! _linearIPlookup->lookup_iproute_w_next_addr(current_route->gw, iter->addr)) {
                
                newiproute.addr = current_route->gw;
                newiproute.mask = netmask32;
                newiproute.gw = iter->addr;
                //newiproute.port = iproute->port;
                newiproute.status = current_route->status;
                newiproute.willingness = current_route->willingness;
                newiproute.hops = 4;
#ifndef ENABLE_ETX
                newiproute.etx = -1;
                newiproute.variance = -1;
                newiproute.etx_time = -1;
                newiproute.var_time = -1;
                newiproute.etx_min = -1;
                newiproute.etx_min_time = -1;
                newiproute.etx_max = -1;
                newiproute.etx_max_time = -1;
#else
                newiproute.etx = current_route->etx;//OLSRMetricETX::sum_etx(iproute->etx, topology->T_N_etx);
                newiproute.variance = current_route->variance;//OLSRMetricVTX::calculate_covtx(iproute->variance, topology->T_N_variance);
                newiproute.var_time = current_route->var_time;
                
                //The values below are based on total etx and not the N_etx
                newiproute.etx_time = current_route->etx_time;
                newiproute.etx_min = current_route->etx_min;//newiproute.etx_min;
                newiproute.etx_min_time = current_route->etx_min_time;//timeComputeTableWasCalled;
                newiproute.etx_max = current_route->etx_max;//newiproute.etx_max;
                newiproute.etx_max_time = current_route->etx_max_time;//newiproute.etx_max_time;
#endif
                _linearIPlookup->add_route( newiproute, false, 0, _errh, true );
            }
        }
        /*
#ifdef profiling
        
        now = Timestamp::now();
        time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
        click_chatter ( "time part 4 %f", time_dif );
        start = Timestamp::now();
#endif
        
        //step 5 - add routes to other nodes' interfaces that have not already been added
        for ( HashMap<IPAddress, void *>::iterator iter = interface_set->begin(); iter != interface_set->end(); iter++ ) {
            interface_data *interface = ( interface_data * ) iter.value();
            //rtable_entry *entry = (rtable_entry *) _routingTable->find(interface->I_main_addr);
            if ( _linearIPlookup->lookup_iproute( interface->I_main_addr )  ) {
                if (! (iproute = _linearIPlookup->lookup_iproute( interface->I_iface_addr ) )) {
                    
                    newiproute.addr = interface->I_iface_addr;
                    newiproute.mask = netmask32;
                    newiproute.gw = iproute->gw;
                    newiproute.port = iproute->port;
                    newiproute.status = iproute->status;//-1;//neighbor->N_status; //Currently, this is unknown, have to see if I want to pass quantity in tc messages
                    newiproute.willingness = iproute->willingness;//OLSR_SYM_NEIGH;//neighbor->N_willingness; //Assumed since two-hop neighbors are in tc messages
                    newiproute.hops = iproute->hops;
#ifndef ENABLE_ETX
                    newiproute.etx = -1;
                    newiproute.variance = -1;
                    newiproute.etx_time = -1;
                    newiproute.var_time = -1;
                    newiproute.etx_min = -1;
                    newiproute.etx_min_time = -1;
                    newiproute.etx_max = -1;
                    newiproute.etx_max_time = -1;
#else
                    newiproute.etx = iproute->etx;
                    newiproute.variance = iproute->variance;
#endif
                    _linearIPlookup->add_route( newiproute, false, 0, _errh );
                }
            }
        }
        if ( _visitorInfo ) {
            _visitorInfo->clear();
            //	click_chatter ("%s | checking for new visitors!!!!\n",_myIP.unparse().c_str());
            for ( OLSRLinearIPLookup::OLSRIPRouteTableIterator iter = _linearIPlookup->begin() ; iter != _linearIPlookup->end(); iter++ ) {
                // click_chatter ("%s | CHECKING if node %s is visiting my network\n",_myIP.unparse().c_str(), _linearIPlookup->_t[index].addr.unparse().c_str());
                if ( iter->mask == netmask32 && !iter->addr.matches_prefix( _myIP, _myMask ) ) { // check if this node is on my subnet
                    _visitorInfo->add_tuple( iter->gw, iter->addr, iter->mask, Timestamp::make_msec( 0, 0 ) );
                    Timestamp now = Timestamp::now();
                    click_chatter ( "%f | %s | node %s is visiting my network\n", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), iter->addr.unparse().c_str() );
                }
            }
        }*/
        
        //step 6 - add routes to entries in the association table
        /// @TODO I don't feel that this code is 100% compliant, this definitely needs to be looked at and possibly rewritten (see p. 54 of the RFC)
        /*for ( HashMap<IPPair, void *>::iterator iter = association_set->begin(); iter != association_set->end(); iter++ ) {
            association_data *association = ( association_data * ) iter.value();
            if ( (iproute = _linearIPlookup->lookup_iproute( association->A_gateway_addr )) ) {
                if ( !_linearIPlookup->lookup_iproute( association->A_network_addr ) *//** @TODO This seems like a weird or. have to check this again!! || _linearIPlookup->lookup_iproute( association->A_network_addr )->gw != iproute->gw *//*) {
                    newiproute.addr = association->A_network_addr;
                    newiproute.mask = association->A_netmask;
                    newiproute.gw = iproute->gw;
                    newiproute.port = iproute->port;
                    newiproute.status = iproute->status;//-1;//neighbor->N_status; //Currently, this is unknown, have to see if I want to pass quantity in tc messages
                    newiproute.willingness = iproute->willingness;//OLSR_SYM_NEIGH;//neighbor->N_willingness; //Assumed since two-hop neighbors are in tc messages
                    newiproute.hops = iproute->hops;
#ifndef ENABLE_ETX
                    newiproute.etx = -1;
                    newiproute.variance = -1;
                    newiproute.etx_time = -1;
                    newiproute.var_time = -1;
                    newiproute.etx_min = -1;
                    newiproute.etx_min_time = -1;
                    newiproute.etx_max = -1;
                    newiproute.etx_max_time = -1;
#else
                    newiproute.etx = iproute->etx;
                    newiproute.variance = iproute->variance;
#endif
                    _linearIPlookup->add_route( newiproute, false, 0, _errh );
                    
                } else {
                    if ( _linearIPlookup->lookup_iproute( association->A_network_addr )->hops > iproute->hops ) {
#ifndef ENABLE_ETX
                        _linearIPlookup->update(association->A_network_addr, iproute->gw, iproute->port, iproute->extra);
#else
                        _linearIPlookup->update(association->A_network_addr, iproute->gw, iproute->port, iproute->extra, iproute->hops, iproute->etx, iproute->variance);
#endif
                    }
                }
            }
        }*/
    }
    



#ifdef profiling

    Timestamp now = Timestamp::now();
	time_dif = ( now.tv_sec - start.tv_sec ) + ( now.tv_usec - start.tv_usec ) / 1E6;
	click_chatter ( "%f | %s | time part 5 %f", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), time_dif );
	click_chatter ( "%f | %s | Routingtable size: %d\n", Timestamp( now ).doubleval(), _myIP.unparse().c_str(), _routingTable->size() );

#endif

	// #ifdef logging
	//   Timestamp now = Timestamp::now();
	//   click_chatter ("%f | %s | ROUTINGTABLE has been computed \n",timeval2double(now), _myIP.unparse().c_str());
	//   print_routing_table();
	//   //  _neighborInfo->print_mpr_set();
	//   _neighborInfo->print_mpr_selector_set();
	//   _topologyInfo->print_topology();
	//   click_chatter ("\n");
	// #endif

}

void
OLSRRoutingTable::clear_table(void)
{
    _linearIPlookup->clear();
}

int
OLSRRoutingTable::add_route_to_table(const OLSRIPRoute &r, bool allow_replace, OLSRIPRoute* replaced, ErrorHandler *errh, bool add_replica_routes)
{
    return _linearIPlookup->add_route(r, allow_replace, replaced, errh, add_replica_routes);
}

IPAddress
OLSRRoutingTable::get_address_from_interface(IPAddress address)
{
    return _interfaceInfo->get_main_address(address);
}

IPAddress
OLSRRoutingTable::get_my_address()
{
    return _myIP;
}

int
OLSRRoutingTable::get_index_from_local_interface(IPAddress address)
{
    return _localIfaces->get_index(address);
}

void
OLSRRoutingTable::set_kroutes(int k_routes)
{
    _kNumOfRoutes = k_routes;
    
    click_chatter ( "OLSRRoutingTable - MyIP: %s, _kNumOfRoutes = %d\n",_myIP.unparse().c_str(), _kNumOfRoutes );
}

void
OLSRRoutingTable::set_route_locally(bool route_locally)
{
    _compute_routing_table_locally = route_locally;
    
    click_chatter ( "OLSRRoutingTable - MyIP: %s, _compute_routing_table_locally = %d\n",_myIP.unparse().c_str(), _compute_routing_table_locally );
}

void
OLSRRoutingTable::add_handlers()
{
    add_write_handler("set_rtable",set_table_handler, 0);
    add_write_handler("set_route_locally", set_route_locally_handler, 0);
    add_write_handler("set_kroutes", set_kroutes_handler, 0);
    add_read_handler("print_rtable", print_table_handler);
    
}

String
OLSRRoutingTable::print_table_handler(Element* e, void *)
{
    OLSRRoutingTable *table = static_cast<OLSRRoutingTable*>(e);
    
    table->print_routing_table();
    return "PRINT_TABLE";
}

int
OLSRRoutingTable::set_kroutes_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
    OLSRRoutingTable* me = static_cast<OLSRRoutingTable*>(e);
    bool new_kroute;
    int res = cp_va_kparse( conf, me, errh, "K_NUM_OF_ROUTES", cpkP+cpkN, cpInteger, &new_kroute, cpEnd);
    if ( res < 0 )
        return res;
    
    me->set_kroutes(new_kroute);
    return res;
}

int
OLSRRoutingTable::set_route_locally_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
    OLSRRoutingTable* me = static_cast<OLSRRoutingTable*>(e);
    bool new_route_locally;
    int res = cp_va_kparse( conf, me, errh, "CREATE_ROUTES_LOCALLY", cpkP+cpkN, cpBool, &new_route_locally, cpEnd);
    if ( res < 0 )
        return res;
    
    me->set_route_locally(new_route_locally);
    return res;
}

int
OLSRRoutingTable::set_table_handler(const String& s, Element* e, void *, ErrorHandler* errh)
{
    OLSRRoutingTable *table = static_cast<OLSRRoutingTable*>(e);
    IPAddress netmask32( "255.255.255.255" );
    Vector<String> original_routes;
    Vector<String> route_fields;
    Vector<String> time_fields;
    size_t sz;
    
    original_routes = cp_parse_string_w_delim(s, '\n');
    //Commented out last line because I removed headers from the controller level
    //original_routes.erase(original_routes.begin()); //Get rid of header row
    
    table->clear_table(); //Clear current table
    for (Vector<String>::iterator it=original_routes.begin(); it != original_routes.end(); it++) {
        
        route_fields = cp_parse_string_w_delim(*it, '\t');
        
        OLSRIPRoute new_iproute;
        
        new_iproute.addr = table->get_address_from_interface(IPAddress(route_fields.at(0)));
        new_iproute.port = table->get_index_from_local_interface(IPAddress(route_fields.at(11))); //This gets interface add of nexthop
        new_iproute.mask = netmask32;
        new_iproute.gw = IPAddress(route_fields.at(1));
        new_iproute.hops = std::stol(route_fields.at(2).c_str());
        new_iproute.etx = std::stod (route_fields.at(3).c_str(), &sz);
        new_iproute.variance = std::stod (route_fields.at(4).c_str(), &sz);
        
        //click_chatter("****COREY - Me: %s Dest: %s, Nexthop: %s, Port: %d, IPofPort: %s", table->get_my_address().unparse().c_str(), new_iproute.addr.unparse().c_str(), new_iproute.gw.unparse().c_str(), new_iproute.port, route_fields.at(11).c_str());
        
        time_fields = cp_parse_string_w_delim(route_fields.at(5), '.');
        if (time_fields.size() > 1)
            new_iproute.etx_time = Timestamp::make_msec(std::stol(time_fields.at(0).c_str()), std::stol(time_fields.at(1).c_str()));
        else
            new_iproute.etx_time = Timestamp::make_msec(std::stol(route_fields.at(5).c_str()));
        
    
        time_fields = cp_parse_string_w_delim(route_fields.at(6), '.');
  
        if (time_fields.size() > 1)
            new_iproute.var_time = Timestamp::make_msec(std::stol(time_fields.at(0).c_str()), std::stol(time_fields.at(1).c_str()));
        else
            new_iproute.var_time = Timestamp::make_msec(std::stol(route_fields.at(6).c_str()));
        
        new_iproute.etx_min = std::stod (route_fields.at(7).c_str(), &sz);
        
        time_fields = cp_parse_string_w_delim(route_fields.at(8), '.');
        //click_chatter("**COREY4 %s", time_fields.at(0).c_str());
        
        if (time_fields.size() > 1)
            new_iproute.etx_min_time = Timestamp::make_msec(std::stol(time_fields.at(0).c_str()), std::stol(time_fields.at(1).c_str()));
        else
            new_iproute.etx_min_time = Timestamp::make_msec(std::stol(route_fields.at(8).c_str()));
        
        new_iproute.etx_max = std::stod (route_fields.at(9).c_str(), &sz);
        
        time_fields = cp_parse_string_w_delim(route_fields.at(10), '.');
        
        if (time_fields.size() > 1)
            new_iproute.etx_max_time = Timestamp::make_msec(std::stol(time_fields.at(0).c_str()), std::stol(time_fields.at(1).c_str()));
        else
            new_iproute.etx_max_time = Timestamp::make_msec(std::stol(route_fields.at(10).c_str()));
       
        new_iproute.status = 0;//route_fields.at();
        new_iproute.willingness = 0;//route_fields.at();
        
        table->add_route_to_table(new_iproute, false, 0, errh, false); //Change last param to true to add mult routes
    }
    
    //table->print_routing_table();
    
    return 0;

}


#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<IPAddress, void *>
;
#endif

CLICK_ENDDECLS

EXPORT_ELEMENT(OLSRRoutingTable);

