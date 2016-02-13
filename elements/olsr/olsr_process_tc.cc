//TED 140504: created

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/ipaddress.hh>
#include "click_olsr.hh"
#include "olsr_process_tc.hh"
#include "olsr_topology_infobase.hh"
#include "olsr_metric_etx.hh"
#include "olsr_metric_vtx.hh"
#include "olsr_metric_delta.hh"


CLICK_DECLS

OLSRProcessTC::OLSRProcessTC()
{
}

OLSRProcessTC::~OLSRProcessTC()
{
}


int
OLSRProcessTC::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _fullTopologyTable = 0;
  if (cp_va_kparse(conf, this, errh, 
		  "TOPOLOGYINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_topologyInfo,
		  "NEIGHBOR_INFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_neighborInfo,
		  "INTERFACEINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_interfaceInfo,
		  "ROUTING_TABLE_ELEMENT", cpkP+cpkM, cpElement, &_routingTable,
		  "MY_MAIN_IP", cpkP+cpkM, cpIPAddress, &_myMainIP,
		  "FULL_TOPOLOGY_TABLE_ELEMENT", cpkN, cpElement, &_fullTopologyTable,
		  cpEnd) < 0)
    return -1;
  return 0;
}

//output 0 - Packets that are to be forwarded
//output 1 - Discard

void
OLSRProcessTC::push(int, Packet *packet)
{
    bool topology_tuple_added = false;
    bool topology_tuple_removed = false;
    bool topology_tuple_updated = false;
    bool two_hop_tuple_updated = false;
    msg_hdr_info msg_info;
    tc_hdr_info tc_info;
    int ansn;
    topology_data *topology_tuple	;
    IPAddress originator_address;
#ifndef ENABLE_ETX
    int size_of_addr_field = sizeof(in_addr);
#else
    int size_of_addr_field = sizeof(in_address_w_metrics_tc);
#endif
    
    Timestamp now = Timestamp::now();
    
    msg_info = OLSRPacketHandle::get_msg_hdr_info(packet, 0);
    tc_info = OLSRPacketHandle::get_tc_hdr_info(packet, (int) sizeof(olsr_msg_hdr));
    originator_address = msg_info.originator_address;
    ansn = tc_info.ansn;
    
    //click_chatter ("node %s Processing TC message with ansn %d from originator address %s\n",_myMainIP.unparse().cc(),ansn,originator_address.unparse().cc());
    click_chatter ("node %s Processing TC message with ansn %d from originator address %s\n",_myMainIP.unparse().c_str(),ansn,originator_address.unparse().c_str());
    //_topologyInfo->print_topology();
    //_neighborInfo->print_neighbor_set();
    //RFC ch 9.5 - TC Msg processing
    //step 1 - discard message if not from symmetric 1-hop neighbor
    IPAddress src_addr = packet->dst_ip_anno();
    neighbor_data *neighbor_info = _neighborInfo->find_neighbor(_interfaceInfo->get_main_address(src_addr));
    
    if (neighbor_info  == 0){
        //click_chatter("Discarded TC message, not from 1-hop neighbor (src address: %s)\n",src_addr.unparse().c_str());//cc());
        output(1).push(packet);
        return;
    }
    //RFC 9.5 step 2 - discard message received out of order
    if ( _topologyInfo->newer_tuple_exists(originator_address, (int) ansn) ){
        //click_chatter("Discarded TC message, received out of order\n");
        output(1).push(packet);
        return;
    }
    //step 3 - delete outdated topology tuples
    if ( _topologyInfo->remove_outdated_tuples(originator_address, (int) ansn) ){
        //click_chatter("topology_tuple_removed\n");
        topology_tuple_removed = true;
    }
    
    //step 4 - record topology tuple
    int remaining_neigh_bytes = msg_info.msg_size - (int)sizeof(olsr_msg_hdr) - (int)sizeof(olsr_tc_hdr);
    int neigh_addr_offset = sizeof(olsr_msg_hdr) + sizeof(olsr_tc_hdr);
    
    while ( remaining_neigh_bytes >= size_of_addr_field ){
#ifndef ENABLE_ETX
        in_addr *address = (in_addr *) (packet->data() + neigh_addr_offset);
        IPAddress dest_addr = IPAddress(*address);
#else
        in_address_w_metrics_tc *address = (in_address_w_metrics_tc *) (packet->data() + neigh_addr_offset);
        in_address_w_metrics_tc addr_w_metrics = *address;
        IPAddress dest_addr = IPAddress(addr_w_metrics.address);
        double cur_neighbor_etx = OLSRMetricETX::decompress(addr_w_metrics.etx);
        double cur_neighbor_vtx = OLSRMetricVTX::decompress(addr_w_metrics.variance);
#endif
        
        //click_chatter("******Destination Addr: %s\t Originator Addr: %s\n", dest_addr.unparse().c_str(), originator_address.unparse().c_str());
        //click_chatter("COREY Remain: %d, Address_size: %d\n", remaining_neigh_bytes, size_of_addr_field);
        
        if (dest_addr != _myMainIP && _neighborInfo->find_neighbor(dest_addr) == 0){
            //dont record entries for myself or my neighbors
            topology_tuple = _topologyInfo->find_tuple(dest_addr, originator_address);
            if ( topology_tuple == 0 ){
                topology_tuple = _topologyInfo->add_tuple(dest_addr, originator_address, (now+msg_info.validity_time));
                topology_tuple->T_seq = ansn;
                
                topology_tuple_added = true;
                
            } else
                topology_tuple->T_time = now + msg_info.validity_time;
            
#ifdef ENABLE_ETX
            if (cur_neighbor_etx < OLSR_METRIC_INFINITY) {
                
                //If ETX is valid, always update to the latest time received, currently doesn't request RT recalculation
                if (cur_neighbor_etx < OLSR_METRIC_INFINITY)
                    topology_tuple->T_N_etx_time = now;
                
    #ifndef ENABLE_VARIANCE
                if (topology_tuple->T_N_etx != cur_neighbor_etx)
    #else
                topology_tuple->T_N_var_time = now;
                //Currently, a new ansn is not generated for new dtimes because they will always change. Assumes nodes can make their own changes w/ own clock times
                if ( (topology_tuple->T_N_etx != cur_neighbor_etx) || (topology_tuple->T_N_variance != cur_neighbor_vtx))
    #endif
                {
                    topology_tuple_updated = true;
                    topology_tuple->T_N_etx = cur_neighbor_etx;
                    //topology_tuple->T_N_etx_time = now;
                    topology_tuple->T_N_etx_delta_time = OLSRMetricDelta::decompress(addr_w_metrics.dtime);
                    
    #ifdef ENABLE_TOPLOGY_TABLE
                    topology_tuple->hops = msg_info.hop_count;
                    topology_tuple->T_N_status = addr_w_metrics.status;
                    topology_tuple->T_N_willingness = addr_w_metrics.willingness;
    #endif
                        
    #ifdef ENABLE_VARIANCE
                    topology_tuple->T_N_variance = cur_neighbor_vtx;
                    //topology_tuple->T_N_var_time = now;
    #endif
                }
                
                //Compare previous neighbor info with new neighbor info only if it was real
                if ((topology_tuple->T_N_etx < topology_tuple->T_N_etx_min) || (topology_tuple->T_N_etx_min == OLSR_METRIC_INFINITY)){
                    topology_tuple->T_N_etx_min = topology_tuple->T_N_etx;
                    topology_tuple->T_N_etx_min_time = topology_tuple->T_N_etx_time;
                }
                
                if ((topology_tuple->T_N_etx > topology_tuple->T_N_etx_max) || (topology_tuple->T_N_etx_max == OLSR_METRIC_INFINITY)){
                    topology_tuple->T_N_etx_max = topology_tuple->T_N_etx;
                    topology_tuple->T_N_etx_max_time = topology_tuple->T_N_etx_time;
                }
               
            }
#endif
            
            //_topologyInfo->print_topology();
        }

#ifdef ENABLE_ETX
        //Added by Corey, not part of RFC
        //Update two hop metrics for ETX. This can only be updated through received tc packets
        if (dest_addr != _myMainIP){
            
            twohop_data *two_hop_neighbor = _neighborInfo->find_twohop_neighbor(originator_address, dest_addr);
            if (two_hop_neighbor != 0){
                //This is data for a two-hop neighbor
                neighbor_data *neighbor_of_two_hop = _neighborInfo->find_neighbor(two_hop_neighbor->N_neigh_main_addr);
                //click_chatter("**Dst: %s, Orig: %s, Two-hop-neighbor: %s Originator: %s Neighbor: %s", dest_addr.unparse().c_str(), originator_address.unparse().c_str(), two_hop_neighbor->N_twohop_addr.unparse().c_str(), originator_address.unparse().c_str(), two_hop_neighbor->N_neigh_main_addr.unparse().c_str());
       
                if (neighbor_of_two_hop != 0) {
                    
                    if (cur_neighbor_etx < OLSR_METRIC_INFINITY) {
                        //If ETX is valid, always update to the latest time received, currently doesn't request RT recalculation
                        two_hop_neighbor->etx_time = now;
                        two_hop_neighbor->N_etx_time = now;
                        
#ifndef ENABLE_VARIANCE
                        if (two_hop_neighbor->N_etx != cur_neighbor_etx)
#else
                        if (neighbor_of_two_hop->etx < OLSR_METRIC_INFINITY ){
                            two_hop_neighbor->N_var_time = now;
                            two_hop_neighbor->var_time = now;
                        }
                        
                        //Currently, a new ansn is not generated for new dtimes because they will always change. Assumes nodes can make their own changes w/ own clock times
                        if ( ((two_hop_neighbor->N_etx != cur_neighbor_etx) || (two_hop_neighbor->variance != cur_neighbor_vtx)) &&(neighbor_of_two_hop->etx < OLSR_METRIC_INFINITY ))
#endif
                        {
                            two_hop_neighbor->etx = OLSRMetricETX::sum_etx(neighbor_of_two_hop->etx, cur_neighbor_etx);
                            two_hop_neighbor->variance = OLSRMetricVTX::calculate_covtx(1/two_hop_neighbor->etx, 1);//OLSRMetricVTX::calculate_covtx(neighbor_of_two_hop->variance, cur_neighbor_vtx);
                            two_hop_neighbor->N_etx = cur_neighbor_etx;
                            two_hop_neighbor->N_etx_delta_time = OLSRMetricDelta::decompress(addr_w_metrics.dtime);
                            two_hop_neighbor->N_variance = cur_neighbor_vtx;
                                
                            //click_chatter("***TWO-HOP-NEIGHBOR Updated| Two-hop: %s Neighbor: %s Two-hop_N_etx: %d incoming etx: %d, neighbor_etx: %d",two_hop_neighbor->N_twohop_addr.unparse().c_str(), two_hop_neighbor->N_neigh_main_addr.unparse().c_str(), two_hop_neighbor->N_etx, cur_neighbor_etx, neighbor_of_two_hop->etx);
                                
                            /*two_hop_neighbor->etx_time = now;
                            two_hop_neighbor->var_time = now;
                            two_hop_neighbor->N_etx_time = now;
                            two_hop_neighbor->N_var_time = now;*/
                            //Compare previous neighbor info with new neighbor info only if it was real
                            if ((two_hop_neighbor->etx < two_hop_neighbor->etx_min) || (two_hop_neighbor->etx_min == OLSR_METRIC_INFINITY)){
                                two_hop_neighbor->etx_min = two_hop_neighbor->etx;
                                two_hop_neighbor->etx_min_time = two_hop_neighbor->etx_time;
                            }
                                
                            if ((two_hop_neighbor->etx > two_hop_neighbor->etx_max) || (two_hop_neighbor->etx_max == OLSR_METRIC_INFINITY)){
                                two_hop_neighbor->etx_max = two_hop_neighbor->etx;
                                two_hop_neighbor->etx_max_time = two_hop_neighbor->etx_time;
                            }
                                
                            //Compare previous neighbor info with new neighbor info only if it was real
                            if ((two_hop_neighbor->N_etx < two_hop_neighbor->N_etx_min) || (two_hop_neighbor->N_etx_min == OLSR_METRIC_INFINITY)){
                                two_hop_neighbor->N_etx_min = two_hop_neighbor->N_etx;
                                two_hop_neighbor->N_etx_min_time = two_hop_neighbor->N_etx_time;
                            }
                                
                            if ((two_hop_neighbor->N_etx > two_hop_neighbor->N_etx_max) || (two_hop_neighbor->N_etx_max == OLSR_METRIC_INFINITY)){
                                two_hop_neighbor->N_etx_max = two_hop_neighbor->N_etx;
                                two_hop_neighbor->N_etx_max_time = two_hop_neighbor->N_etx_time;
                            }
                            
                            two_hop_tuple_updated = true;
                        }
                    }
                        
                }
            }
    
        }
#endif
 
        remaining_neigh_bytes -= size_of_addr_field;
        neigh_addr_offset += size_of_addr_field;
    }
        
    if ( topology_tuple_added || topology_tuple_removed || topology_tuple_updated || two_hop_tuple_updated){
        
        _routingTable->compute_routing_table(false,now);
        _routingTable->print_routing_table();
        if (_fullTopologyTable != 0){
            _fullTopologyTable->compute_routing_table(true, now);
            _fullTopologyTable->print_routing_table(true);
        }
    }
        
    output(0).push(packet);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRProcessTC);

