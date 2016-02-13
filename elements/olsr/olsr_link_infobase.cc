//TED 160404: Created
//Partially based on grid/linktable - implementation

#include <click/config.h>
#include <click/confparse.hh>
#include "olsr_link_infobase.hh"
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
//#include "ippair.hh"
#include "../wifi/linktable.hh"
#include "click_olsr.hh"

CLICK_DECLS

OLSRLinkInfoBase::OLSRLinkInfoBase()
		: _timer(this)
{
}


OLSRLinkInfoBase::~OLSRLinkInfoBase()
{
}


int
OLSRLinkInfoBase::configure (Vector<String> &conf, ErrorHandler *errh)
{
    _fullTopologyTable = 0;
        if (cp_va_kparse(conf, this, errh,
	                "NEIGHBORINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_neighborInfo,
	                "INTERFACINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_interfaceInfo,
	                "DUPLICATE_SET_ELEMENT", cpkP+cpkM, cpElement, &_duplicateSet,
	                "ROUTING_TABLE_ELEMENT", cpkP+cpkM, cpElement, &_routingTable,
	                "TC_GENERATOR_ELEMENT", cpkP+cpkM, cpElement, &_tcGenerator,
                    "FULL_TOPOLOGY_TABLE_ELEMENT", cpkN, cpElement, &_fullTopologyTable,
                         cpEnd) < 0)
		return -1;
	return 0;
}


int
OLSRLinkInfoBase::initialize(ErrorHandler *)
{
	_timer.initialize(this);
	_linkSet = new LinkSet();		//ok new
	return 0;
}

void OLSRLinkInfoBase::uninitialize()
{
	delete _linkSet;
}

void
OLSRLinkInfoBase::run_timer(Timer *)
{
	bool neighbor_removed = false;
	bool mpr_selector_removed=false;
	bool neighbor_downgraded = false;
    Timestamp now = Timestamp::now();
    Timestamp time;
	Timestamp next_timeout = Timestamp::make_msec(0,0);
	IPAddress neighbor;
//	neighbor_data *neighbor_entry;
	HashMap<IPAddress, IPAddress> links_removed_obj;
	HashMap<IPAddress, IPAddress> *links_removed = &links_removed_obj;
	HashMap<IPAddress, IPAddress> links_downgraded_obj;
	HashMap<IPAddress, IPAddress> *links_downgraded = &links_downgraded_obj;

	//find expired links and remove them
	if (! _linkSet->empty() )
	{
		for (LinkSet::iterator iter = _linkSet->begin(); iter != _linkSet->end(); iter++)
		{
			link_data *data = (link_data *)iter.value();
			//store the main address of the node to which this was a link
			neighbor = data->_main_addr; //_interfaceInfo->get_main_address(data->L_neigh_iface_addr);	
			if (data->L_time <= now)
			{
				links_removed->insert(data->L_neigh_iface_addr, neighbor);
				//click_chatter("link %s <--> %s expired | %d %d\n", data->L_local_iface_addr.unparse().c_str(), data->L_neigh_iface_addr.unparse().c_str(), data->L_time.tv_sec, data->L_time.tv_usec);
                click_chatter("link %s <--> %s expired | %f\n", data->L_local_iface_addr.unparse().c_str(), data->L_neigh_iface_addr.unparse().c_str(), Timestamp(data->L_time).doubleval());
				remove_link(data->L_local_iface_addr, data->L_neigh_iface_addr);
			}
			else if (data->L_SYM_time <= now)
			{
				links_downgraded->insert(data->L_neigh_iface_addr, neighbor);
// 				neighbor_data* nbr_entry = _neighborInfo->find_neighbor(neighbor);
// 				if (nbr_entry->N_status == OLSR_SYM_NEIGH || nbr_entry->N_status == OLSR_MPR_NEIGH)
// 				{
				
                click_chatter("link %s <--> %s no longer symmetric | %f\n", data->L_local_iface_addr.unparse().c_str(), data->L_neigh_iface_addr.unparse().c_str(), Timestamp(data->L_SYM_time).doubleval());
//					neighbor = _interfaceInfo->get_main_address(data->L_neigh_iface_addr);
// 					nbr_entry->N_status=OLSR_NOT_NEIGH;
// 					neighbor_downgraded = true;
// 				}
			}
		}
	}

	//remove neighbors that have no more links
	if (! links_removed->empty())
	{
		neighbor_removed = true;
		for( HashMap<IPAddress, IPAddress>::iterator iter = links_removed->begin(); iter != links_removed->end(); iter++)
		{
			bool other_links_left = false;
			for (LinkSet::iterator link = _linkSet->begin(); link != _linkSet->end(); link++)
			{
				link_data *data = (link_data *)link.value();
				if (_interfaceInfo->get_main_address(data->L_neigh_iface_addr) == iter.value() && data->L_SYM_time > now)
				{
					other_links_left = true;
				}
			}
			if (!other_links_left) {
				_neighborInfo->remove_neighbor(iter.value());
				neighbor_removed = true;
				if (_neighborInfo->find_mpr_selector (iter.value()))
				{
					_neighborInfo->remove_mpr_selector(iter.value());
					mpr_selector_removed=true;
				}
			}
		}
	}

	//downgrade neighbors that have no more symmetric links
	if (! links_downgraded->empty() ) {
		neighbor_downgraded = true;
		for( HashMap<IPAddress, IPAddress>::iterator iter = links_downgraded->begin(); iter != links_downgraded->end(); iter++)
		{
			bool sym_link_left = false;
			for (LinkSet::iterator link = _linkSet->begin(); link != _linkSet->end(); link++)
			{
				link_data *data = (link_data *)link.value();
				if (_interfaceInfo->get_main_address(data->L_neigh_iface_addr) == iter.value() && data->L_SYM_time > now)
				{
					sym_link_left = true;
				}
			}
			if (!sym_link_left) {
				neighbor_data* nbr_entry = _neighborInfo->find_neighbor(iter.value());
				nbr_entry->N_status=OLSR_NOT_NEIGH;
				neighbor_downgraded = true;
				if (_neighborInfo->find_mpr_selector (iter.value()))
				{
					_neighborInfo->remove_mpr_selector(iter.value());
					mpr_selector_removed=true;
				}
			}
		}
	}

	//find next link to expire
	if (! _linkSet->empty() )
	{
		for (LinkSet::iterator iter = _linkSet->begin(); iter != _linkSet->end(); iter++)
		{
			link_data *data = (link_data *)iter.value();

			neighbor = data->_main_addr;

			if (_neighborInfo->find_neighbor(neighbor)->N_status == OLSR_SYM_NEIGH)
			{
				time = data->L_SYM_time;
			}
			else
			{
				time = data->L_time;
			}
			//if (next_timeout.tv_sec == 0 && next_timeout.tv_usec == 0)
            if (next_timeout == 0)
			{
				next_timeout = time;
			}
			if ( time < next_timeout )
			{
				next_timeout = time;
			}
		}
	}

	if (mpr_selector_removed) _tcGenerator->notify_mpr_selector_changed();
	//if (! (next_timeout.tv_sec == 0 && next_timeout.tv_usec == 0) )
    if (next_timeout != 0)
		_timer.schedule_at(next_timeout);    //set timer
	if (neighbor_removed || neighbor_downgraded)
	{
		_neighborInfo->compute_mprset();
		_routingTable->compute_routing_table(false, now);
        if (_fullTopologyTable != 0){
            _fullTopologyTable->compute_routing_table(true, now);
            //_fullTopologyTable->print_routing_table(true);
        }
	}
}

link_data *
OLSRLinkInfoBase::add_link(IPAddress local_addr, IPAddress neigh_addr, Timestamp time, Timestamp time_metric_enabled, Timestamp time_next_hello_should_arrive)
{
	IPPair ippair=IPPair(local_addr, neigh_addr);;
	struct link_data *data;
	data = new struct link_data;		//memory freed in remove

	data->L_local_iface_addr = local_addr;
	data->L_neigh_iface_addr = neigh_addr;
	data->L_time = time;
	data->L_time_metric_enabled = time_metric_enabled;
    data->L_time_next_hello_should_arrive = time_next_hello_should_arrive;
	data->L_num_of_pkts_rec = 1;
	data->L_rec_enough_pkts_to_use_metric = false;
    data->etx = OLSR_METRIC_INFINITY;
    data->variance = OLSR_METRIC_INFINITY;
    data->etx_time = Timestamp::make_msec(0,0);
    data->var_time = Timestamp::make_msec(0,0);
    data->etx_min = OLSR_METRIC_INFINITY;
    data->etx_min_time = Timestamp::make_msec(0,0);
    data->etx_max = OLSR_METRIC_INFINITY;
    data->etx_max_time = Timestamp::make_msec(0,0);
    
	//click_chatter("link %s <--> %s insert | Time: %d %d | TimeMetricEnabled: %d %d | TimeNextHelloShouldArrive: %d %d | NumOfPktsRec: %d | EnoughPktsToUseMetric: %d\n", data->L_local_iface_addr.unparse().c_str(), data->L_neigh_iface_addr.unparse().c_str(), data->L_time.tv_sec, data->L_time.tv_usec, data->L_time_metric_enabled.tv_sec, data->L_time_metric_enabled.tv_usec, data->L_time_next_hello_should_arrive.tv_sec, data->L_time_next_hello_should_arrive.tv_usec, data->L_num_of_pkts_rec, data->L_rec_enough_pkts_to_use_metric);
    click_chatter("link %s <--> %s insert | Time: %f | ValidUntil: %f | TimeMetricEnabled: %f | TimeNextHelloShouldArrive: %f | NumOfPktsRec: %d | EnoughPktsToUseMetric: %d\n", data->L_local_iface_addr.unparse().c_str(), data->L_neigh_iface_addr.unparse().c_str(), Timestamp::now().doubleval(), Timestamp(data->L_time).doubleval(), Timestamp(data->L_time_metric_enabled).doubleval(), Timestamp(data->L_time_next_hello_should_arrive).doubleval(), data->L_num_of_pkts_rec, data->L_rec_enough_pkts_to_use_metric);

	if (_linkSet->empty())
		_timer.schedule_at(time);
	if (_linkSet->insert(ippair, data) ) {
		return data;
	}
	return 0;
}


link_data*
OLSRLinkInfoBase::find_link(IPAddress local_addr, IPAddress neigh_addr)
{
	if (! _linkSet->empty() )
	{
		IPPair ippair = IPPair(local_addr, neigh_addr);
		link_data *data = (link_data *) _linkSet->find(ippair);

		if ( data != 0 )
			return data;
	}
	return 0;
}


bool
OLSRLinkInfoBase::update_link(IPAddress local_addr, IPAddress neigh_addr, Timestamp sym_time, Timestamp asym_time, Timestamp time, Timestamp time_metric_enabled, int num_of_pkts_rec, bool rec_enough_pkts_to_use_metric)
{
	link_data *data;
	data = find_link(local_addr, neigh_addr);
	if (data != 0 )
	{
		data->L_SYM_time = sym_time;
		data->L_ASYM_time = asym_time;
		data->L_time = time;
		data->L_time_metric_enabled = time_metric_enabled;
		data->L_num_of_pkts_rec = num_of_pkts_rec;
		data->L_rec_enough_pkts_to_use_metric = rec_enough_pkts_to_use_metric;

		//click_chatter("link %s <--> %s updating| %d %d %d\n", data->L_local_iface_addr.unparse().c_str(), data->L_neigh_iface_addr.unparse().c_str(), data->L_time.tv_sec, data->L_time.tv_usec, data->L_etx);
		//click_chatter("link %s <--> %s insert | Time: %d %d | TimeMetricEnabled: %d %d | TimeNextHelloShouldArrive: %d %d | NumOfPktsRec: %d | EnoughPktsToUseMetric: %d\n", data->L_local_iface_addr.unparse().c_str(), data->L_neigh_iface_addr.unparse().c_str(), data->L_time.tv_sec, data->L_time.tv_usec, data->L_time_metric_enabled.tv_sec, data->L_time_metric_enabled.tv_usec, data->L_time_next_hello_should_arrive.tv_sec, data->L_time_next_hello_should_arrive.tv_usec, data->L_num_of_pkts_rec, data->L_rec_enough_pkts_to_use_metric);
        //click_chatter("link %s <--> %s insert | Time: %f | TimeMetricEnabled: %f | TimeNextHelloShouldArrive: %f | NumOfPktsRec: %d | EnoughPktsToUseMetric: %d\n", data->L_local_iface_addr.unparse().c_str(), data->L_neigh_iface_addr.unparse().c_str(), data->L_time, data->L_time_metric_enabled, data->L_time_next_hello_should_arrive, data->L_num_of_pkts_rec, data->L_rec_enough_pkts_to_use_metric);

		_timer.schedule_now();
		return true;
	}
	return false;
}


void
OLSRLinkInfoBase::remove_link(IPAddress local_addr, IPAddress neigh_addr)
{
	IPPair ippair = IPPair(local_addr, neigh_addr);
	link_data *ptr=(link_data*) _linkSet->find(ippair);
	
	//click_chatter("link %s <--> %s removing| %d %d %d\n", ptr->L_local_iface_addr.unparse().c_str(), ptr->L_neigh_iface_addr.unparse().c_str(), ptr->L_time.tv_sec, ptr->L_time.tv_usec, ptr->L_etx);
	//click_chatter("link %s <--> %s removing | Time: %d %d | TimeMetricEnabled: %d %d | TimeNextHelloShouldArrive: %d %d | NumOfPktsRec: %d | EnoughPktsToUseMetric: %d\n", ptr->L_local_iface_addr.unparse().c_str(), ptr->L_neigh_iface_addr.unparse().c_str(), ptr->L_time.tv_sec, ptr->L_time.tv_usec, ptr->L_time_metric_enabled.tv_sec, ptr->L_time_metric_enabled.tv_usec, ptr->L_time_next_hello_should_arrive.tv_sec, ptr->L_time_next_hello_should_arrive.tv_usec, ptr->L_num_of_pkts_rec, ptr->L_rec_enough_pkts_to_use_metric);
    click_chatter("link %s <--> %s removing | Time: %f | ValidUntil: %f | TimeMetricEnabled: %f | TimeNextHelloShouldArrive: %f | NumOfPktsRec: %d | EnoughPktsToUseMetric: %d\n", ptr->L_local_iface_addr.unparse().c_str(), ptr->L_neigh_iface_addr.unparse().c_str(), Timestamp::now().doubleval(), Timestamp(ptr->L_time).doubleval(), Timestamp(ptr->L_time_metric_enabled).doubleval(), Timestamp(ptr->L_time_next_hello_should_arrive).doubleval(), ptr->L_num_of_pkts_rec, ptr->L_rec_enough_pkts_to_use_metric);
	
// 	_interfaceInfo->remove_interfaces_from(neigh_addr);

	_linkSet->remove(ippair);
	delete ptr;

	//reset the packet seq num from this interface, node might be down
	_duplicateSet->remove_packet_seq(neigh_addr);

}

HashMap<IPPair, void*> *
OLSRLinkInfoBase::get_link_set()
{
	return _linkSet;
}


void
OLSRLinkInfoBase::print_link_set()
{
	if (! _linkSet->empty() )
	{
		for (LinkSet::iterator iter = _linkSet->begin(); iter != _linkSet->end(); iter++)
		{
			link_data *data = (link_data *) iter.value();
			click_chatter("link:\n");
			click_chatter("\tlocal_iface: %s\n", data->L_local_iface_addr.unparse().c_str());
			click_chatter("\tneigh_iface: %s\n", data->L_neigh_iface_addr.unparse().c_str());
			click_chatter("\tL_SYM_time: %f\n", Timestamp(data->L_SYM_time).doubleval());
			click_chatter("\tL_ASYM_time: %f\n", Timestamp(data->L_ASYM_time).doubleval());
			click_chatter("\tL_time: %f\n", Timestamp(data->L_time).doubleval());
			click_chatter("\tL_num_of_pkts_rec: %d\n", data->L_num_of_pkts_rec);
			click_chatter("\tL_rec_enough_pkts_to_use_metric: %d\n", data->L_rec_enough_pkts_to_use_metric);
		}
	}
	else
	{
		click_chatter("LinkSet is empty\n");
	}
}



#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<IPPair, void *>;
template class HashMap<IPPair, void *>::iterator;
#endif

CLICK_ENDDECLS

EXPORT_ELEMENT(OLSRLinkInfoBase);

