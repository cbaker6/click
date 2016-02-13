//TED 220404: Created
#include <click/config.h>
#include <click/confparse.hh>
#include "olsr_topology_infobase.hh"
#include <click/ipaddress.hh>
//#include "ippair.hh"
#include "../wifi/linktable.hh"
#include "click_olsr.hh"

CLICK_DECLS

OLSRTopologyInfoBase::OLSRTopologyInfoBase()
  : _timer(this)
{
}


OLSRTopologyInfoBase::~OLSRTopologyInfoBase()
{
}


int
OLSRTopologyInfoBase::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _fullTopologyTable = 0;
  if ( cp_va_kparse( conf, this, errh,
		    "ROUTING_TABLE_ELEMENT", cpkP+cpkM, cpElement, &_routingTable,
            "FULL_TOPOLOGY_TABLE_ELEMENT", cpkN, cpElement, &_fullTopologyTable,
		    cpEnd) < 0 )
    return -1;
  return 0;
}


int
OLSRTopologyInfoBase::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _topologySet = new TopologySet;	//ok
  return 0;
}

void OLSRTopologyInfoBase::uninitialize()
{
 delete _topologySet;
}

topology_data *
OLSRTopologyInfoBase::add_tuple(IPAddress dest_addr, IPAddress last_addr, Timestamp time)
{
    IPPair ippair= IPPair(dest_addr, last_addr);;
    struct topology_data *data;
    data = new struct topology_data;		//released in remove
    
    data->T_dest_addr = dest_addr;
    data->T_last_addr = last_addr;
    data->T_time = time;
    data->T_N_etx = OLSR_METRIC_INFINITY;
    data->T_N_variance = 0;
    data->T_etx = OLSR_METRIC_INFINITY;
    data->T_variance = 0;
    data->T_N_status = -1;
    data->T_N_willingness = -1;
    data->etx_time = Timestamp::make_msec(0,0);
    data->var_time = Timestamp::make_msec(0,0);
    data->etx_min = OLSR_METRIC_INFINITY;
    data->etx_min_time = Timestamp::make_msec(0,0);
    data->etx_max = OLSR_METRIC_INFINITY;
    data->etx_max_time = Timestamp::make_msec(0,0);
    
    data->T_N_etx_time = Timestamp::make_msec(0,0);
    data->T_N_var_time = Timestamp::make_msec(0,0);
    data->T_N_etx_min = OLSR_METRIC_INFINITY;
    data->T_N_etx_min_time = Timestamp::make_msec(0,0);
    data->T_N_etx_max = OLSR_METRIC_INFINITY;
    data->T_N_etx_max_time = Timestamp::make_msec(0,0);
    data->T_N_etx_delta_time = Timestamp::make_msec(0,0);
    
    if ( _topologySet->empty() )
        _timer.schedule_at(time);
    if ( _topologySet->insert(ippair, data) )
        return data;
    
    return 0;
}


topology_data *
OLSRTopologyInfoBase::find_tuple(IPAddress dest_addr, IPAddress last_addr)
{
  if (! _topologySet->empty() ) {
    IPPair ippair = IPPair(dest_addr, last_addr);
    struct HashMap<IPPair, void*>::Pair *pair;
    pair = _topologySet->find_pair(ippair);
    
    if (! pair == 0 ){
      topology_data *data = (topology_data *)pair->value;
      return data;
    }
  }
  
  return NULL;
}


bool
OLSRTopologyInfoBase::newer_tuple_exists(IPAddress last_addr, int ansn)
{
  for (TopologySet::iterator iter = _topologySet->begin(); iter != _topologySet->end(); iter++){
    topology_data *data = (topology_data *) iter.value();
    if (data->T_last_addr == last_addr && data->T_seq > ansn)
      return true;
  }
  return false;
}

void OLSRTopologyInfoBase::print_topology()
{
click_chatter ("TOPOLOGY SET\n");
 for (TopologySet::iterator iter = _topologySet->begin(); iter != _topologySet->end(); iter++){
    topology_data *data = (topology_data *) iter.value();
    click_chatter ("T_dest: %s\t T_last: %s\tT_seq: %d\tT_N_etx: %d\tT_N_variance: %d\tT_etx: %d\tT_variance: %d\n",data->T_dest_addr.unparse().c_str(),data->T_last_addr.unparse().c_str(),data->T_seq,data->T_N_etx,data->T_N_variance,data->T_etx,data->T_variance);
 }
}
 
    
    
   
bool
OLSRTopologyInfoBase::remove_outdated_tuples(IPAddress last_addr, int ansn)
{
  bool tuple_removed = false;
  for(TopologySet::iterator iter =_topologySet->begin(); iter != _topologySet->end(); iter++){
    topology_data *data = (topology_data *) iter.value();
    if (data->T_last_addr == last_addr && data->T_seq < ansn){
      remove_tuple(data->T_dest_addr, data->T_last_addr);
      tuple_removed = true;
    }
  }
  return tuple_removed;
} 


void
OLSRTopologyInfoBase::remove_tuple(IPAddress dest_addr, IPAddress last_addr)
{
 
  IPPair ippair = IPPair(dest_addr, last_addr);
  topology_data *ptr=(topology_data *)_topologySet->find(ippair);
  _topologySet->remove(ippair);
  delete ptr;
}


HashMap<IPPair, void*> *
OLSRTopologyInfoBase::get_topology_set()
{
  return _topologySet;
}


void
OLSRTopologyInfoBase::run_timer(Timer *)
{
    bool topology_tuple_removed = false;
    
    Timestamp now = Timestamp::now();
    Timestamp next_timeout = Timestamp::make_msec(0, 0);
    
    //find expired topology tuple and delete them
    if (! _topologySet->empty()){
        for (TopologySet::iterator iter = _topologySet->begin(); iter != _topologySet->end(); iter++){
            topology_data *tuple = (topology_data *) iter.value();
            if (tuple->T_time <= now){
                remove_tuple(tuple->T_dest_addr, tuple->T_last_addr);
                //click_chatter("Topology tuple expired");
                topology_tuple_removed = true;
            }
        }
    }
    
    //find next topology tuple to expire
    if (! _topologySet->empty()){
        for (TopologySet::iterator iter = _topologySet->begin(); iter != _topologySet->end(); iter++){
            topology_data *tuple = (topology_data *) iter.value();
            //if (next_timeout.tv_sec == 0 && next_timeout.tv_usec == 0)
            if (next_timeout == 0)
                next_timeout = tuple->T_time;
            if ( tuple->T_time < next_timeout )
                next_timeout = tuple->T_time;
        }
    }
    
    //if (! (next_timeout.tv_sec == 0 && next_timeout.tv_usec == 0) )
    if (next_timeout != 0)
        _timer.schedule_at(next_timeout);    //set timer
    if (topology_tuple_removed){
        //  click_chatter("recomputing routing table");
        _routingTable->compute_routing_table(false,now);
        if (_fullTopologyTable != 0){
            _fullTopologyTable->compute_routing_table(true, now);
            //_fullTopologyTable->print_routing_table(true);
        }
        //_routingTable->print_routing_table();
    }
}  


#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<IPPair, void *>;
#endif

CLICK_ENDDECLS

EXPORT_ELEMENT(OLSRTopologyInfoBase);

