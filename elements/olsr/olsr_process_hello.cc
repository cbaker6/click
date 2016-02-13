//TED 150404: created

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/ipaddress.hh>
#include <click/packet_anno.hh>
#include "click_olsr.hh"
#include "olsr_neighbor_infobase.hh"
#include "olsr_process_hello.hh"
#include "olsr_packethandle.hh"
#include "olsr_metric_etx.hh"
#include "olsr_metric_vtx.hh"
#include "clicknet/ether.h"

CLICK_DECLS

OLSRProcessHello::OLSRProcessHello()
{
}

OLSRProcessHello::~OLSRProcessHello()
{
}

int
OLSRProcessHello::configure(Vector<String> &conf, ErrorHandler *errh)
{
	//int neighbor_hold_time;
    _fullTopologyTable = 0;

 	if (cp_va_kparse(conf, this, errh,
	                "NEIGHBOR_HOLD_TIME", cpkP+cpkM, cpInteger, &_neighbor_hold_time,
	                "LINKINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_linkInfo,
	                "NEIGHBORINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_neighborInfo,
	                "INTERFACEINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_interfaceInfo,
	                "ROUTING_TABLE_ELEMENT", cpkP+cpkM, cpElement, &_routingTable,
	                "TC_GENERATOR_ELEMENT", cpkP+cpkM, cpElement, &_tcGenerator,
	                "LOCALIFINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_localIfInfoBase,
	                "MAIN_IPADDRESS_OF_NODE", cpkP+cpkM, cpIPAddress, &_myMainIp,
                    "HELLO_PERIOD", cpkP+cpkN, cpInteger, &_hello_period,
                    "LQ_WINDOW", cpkP+cpkN, cpInteger, &_lq_window_size,
                    "FULL_TOPOLOGY_TABLE_ELEMENT", cpkN, cpElement, &_fullTopologyTable,
                     cpEnd) < 0)
		return -1;
	/*_neighbor_hold_time_tv = Timestamp::make_msec((int) (neighbor_hold_time / 1000),(neighbor_hold_time % 1000));
	//Added by Corey
	_lq_window_time_tv=Timestamp::make_msec ((int) (_hello_period *_lq_window_size / 1000),(_hello_period*_lq_window_size % 1000));
    _hello_time_tv=Timestamp::make_msec ((int) (_hello_period / 1000),(_hello_period % 1000));*/
    compute_htimes();
	return 0;
}

void
OLSRProcessHello::push(int, Packet *packet)
{

	msg_hdr_info msg_info;
	hello_hdr_info hello_info;
	link_hdr_info link_info;
	link_data *link_tuple;
	neighbor_data *neighbor_tuple;

	bool update_twohop = false;
	bool twohop_deleted = false;
	bool new_twohop_added = false;
	bool new_neighbor_added = false;
    bool neighbor_links_updated = false;
	bool mpr_selector_added = false;
    bool mpr_selector_links_updated = false;
    
	IPAddress neighbor_main_address, originator_address, source_address;
    Timestamp now = Timestamp::now();
	msg_info = OLSRPacketHandle::get_msg_hdr_info(packet, 0);

	//click_chatter ("Process HELLO: validity time %d %d",  msg_info.validity_time.tv_sec,msg_info.validity_time.tv_usec);
	originator_address = msg_info.originator_address;
	//dst_ip_anno must be set, must be source address of ippacket
	source_address = packet->dst_ip_anno();

	int paint=static_cast<int>(PAINT_ANNO(packet));//packets get marked with paint 0..N depending on Interface they arrive on
	IPAddress receiving_If_IP=_localIfInfoBase->get_iface_addr(paint); //gets IP of Interface N
	//7.1.1 - 1
	link_tuple = _linkInfo->find_link(receiving_If_IP, source_address);

	if (link_tuple == NULL)
	{  
#ifndef ENABLE_ETX
		link_tuple = _linkInfo->add_link(receiving_If_IP, source_address, (now + msg_info.validity_time));
#else
		link_tuple = _linkInfo->add_link(receiving_If_IP, source_address, (now + msg_info.validity_time), (now + _lq_window_time_tv), (now + _hello_time_tv));
#endif
		link_tuple->L_SYM_time = now - Timestamp::make_msec(1,0);
		link_tuple->L_ASYM_time = now + msg_info.validity_time;
		link_tuple->_main_addr = originator_address;
		click_chatter("%s adding link to %s\n", receiving_If_IP.unparse().c_str(), source_address.unparse().c_str());
	}
	else
	{
		link_tuple->L_ASYM_time = now + msg_info.validity_time;
		if ( _interfaceInfo->get_main_address(link_tuple->L_neigh_iface_addr) == originator_address)
		{ // From RFC 8.2.1
			if (link_tuple->L_SYM_time >= now)
				update_twohop = true;
		}

#ifdef ENABLE_ETX
		//Update link metrics
        if ((now >= link_tuple->L_time_metric_enabled)){
            if (!link_tuple->L_rec_enough_pkts_to_use_metric)
                link_tuple->L_rec_enough_pkts_to_use_metric = true;
        }
        
        if ((now <= link_tuple->L_time_next_hello_should_arrive)){
            link_tuple->L_num_of_pkts_rec += 1;
            link_tuple->L_time_next_hello_should_arrive += _hello_time_tv;
            
            //click_chatter("*****COREY Number of Packers Received %d NOW: %d ShouldArrived: %d HELLOPERIOD %d",link_tuple->L_num_of_pkts_rec, now.tv_sec, link_tuple->L_time_next_hello_should_arrive.tv_sec, _hello_time_tv.tv_sec);

        }else{
            Timestamp time_diff = now - link_tuple->L_time_next_hello_should_arrive;
            
            int pkts_rec_lost = (int)(time_diff / _hello_time_tv);
            
            if (pkts_rec_lost == 0){
                link_tuple->L_num_of_pkts_rec -= 1;
                pkts_rec_lost = 1;
            }else{
                //Time subtraction does something weird subtraction and sometimes makes this number negative, this accounts for it
                if (pkts_rec_lost > 0)
                    link_tuple->L_num_of_pkts_rec -= pkts_rec_lost;
                else
                    link_tuple->L_num_of_pkts_rec += pkts_rec_lost;
            }
            
            Timestamp time_missed = Timestamp::make_msec ((int) ((pkts_rec_lost * _hello_period) / 1000),((pkts_rec_lost * _hello_period) % 1000));
            
            //click_chatter("*****COREY Number of Packets Received %d NumberOfPktsRecLost: %d NOW: %d %d ShouldArrived: %d %d TimeDiff: %d %d TimeMissed: %d %d",link_tuple->L_num_of_pkts_rec, pkts_rec_lost, now.tv_sec, now.tv_usec, link_tuple->L_time_next_hello_should_arrive.tv_sec, link_tuple->L_time_next_hello_should_arrive.tv_usec, time_diff.tv_sec, time_diff.tv_usec, time_missed.tv_sec, time_missed.tv_usec);
            
            link_tuple->L_time_next_hello_should_arrive = link_tuple->L_time_next_hello_should_arrive + time_missed + _hello_time_tv;
		}
        
        if (link_tuple->L_num_of_pkts_rec < 1)
            link_tuple->L_num_of_pkts_rec = 1;
        else if (link_tuple->L_num_of_pkts_rec > _lq_window_size)
            link_tuple->L_num_of_pkts_rec = _lq_window_size;
        
        //link_tuple->L_time_next_hello_should_arrive = now + _lq_window_time_tv;
        
        //click_chatter("*****COREY Number of Packers Received %d NextHelloWindowArrive: %d %d",link_tuple->L_num_of_pkts_rec, link_tuple->L_time_next_hello_should_arrive.tv_sec, link_tuple->L_time_next_hello_should_arrive.tv_usec);
#endif
	}
	hello_info = OLSRPacketHandle::get_hello_hdr_info(packet, sizeof(olsr_msg_hdr));
	//from RFC 8.1
	neighbor_main_address = originator_address;
	neighbor_tuple = _neighborInfo->find_neighbor(neighbor_main_address);
	if (neighbor_tuple == NULL)
	{
		neighbor_tuple = _neighborInfo->add_neighbor(neighbor_main_address);
		neighbor_tuple->N_status = OLSR_NOT_NEIGH;
	}
	neighbor_tuple->N_willingness = hello_info.willingness;

#ifdef ENABLE_ETX
    double current_link_quality = OLSRMetricETX::mean_lq(link_tuple, _lq_window_size);//calculate_average_lq(*link_tuple);
    double current_link_variance = 0;
    
    //#ifdef ENABLE_VARIANCE
    //current_link_variance = OLSRMetricVTX::variance_lq(link_tuple, _lq_window_size); //calculate_variance_lq(*link_tuple);
    //#endif
	//click_chatter("*****COREY LQ: %d",neighbor_tuple->link_quality);
#endif

	//end 8.1
	// end 7.1.1
	int link_msg_bytes_left = msg_info.msg_size - sizeof(olsr_msg_hdr) - sizeof(olsr_hello_hdr);
	int link_msg_offset = sizeof(olsr_msg_hdr) + sizeof(olsr_hello_hdr);
	int olsr_link_hdr_size = sizeof(olsr_link_hdr);
	int address_offset = link_msg_offset + olsr_link_hdr_size;

	if (link_msg_bytes_left > 0)
	{ //there are advertised links in the hello-message
#ifndef ENABLE_ETX
		int size_of_address_field = sizeof(in_addr);
#else
		int size_of_address_field = sizeof(in_address_w_metrics_hello);
#endif

		do
		{
			link_info = OLSRPacketHandle::get_link_hdr_info(packet, link_msg_offset);
			int interface_address_bytes_left = link_info.link_msg_size - olsr_link_hdr_size;
			do
			{
#ifndef ENABLE_ETX
				in_addr *address = (in_addr *) (packet->data() + address_offset);
				IPAddress neighbor_address = IPAddress(*address);
#else
				in_address_w_metrics_hello *address = (in_address_w_metrics_hello *) (packet->data() + address_offset);
				in_address_w_metrics_hello addr_w_metrics = *address;				
				IPAddress neighbor_address = IPAddress(addr_w_metrics.address);
				double neighbor_link_quality = OLSRMetricETX::decompress(addr_w_metrics.link_quality);
                //double neighbor_link_variance = OLSR_METRIC_INFINITY;
    #ifdef ENABLE_VARIANCE
                //neighbor_link_variance = OLSRMetricVTX::decompress(addr_w_metrics.variance);
    #endif
#endif
				
				//from RFC 7.1.1 - 2
				if (neighbor_address == receiving_If_IP)
				{
					if (link_info.link_type == OLSR_LOST_LINK)
					{
						link_tuple->L_SYM_time = now - Timestamp::make_msec(1,0);  // == expired
						link_tuple->L_num_of_pkts_rec = 0; //If link lost, packets received should become 0
                        link_tuple->L_rec_enough_pkts_to_use_metric = false;
					}
					else if (link_info.link_type == OLSR_SYM_LINK || link_info.link_type == OLSR_ASYM_LINK)
					{
						link_tuple->L_SYM_time = now + msg_info.validity_time;
						link_tuple->L_time = link_tuple->L_SYM_time + _neighbor_hold_time_tv;
                        
					}

					if (link_tuple->L_time < link_tuple->L_ASYM_time)
					{
						link_tuple->L_time = link_tuple->L_ASYM_time;
					}

					//from RFC 8.1
					if ( link_tuple->L_SYM_time >= now )
					{
						if (neighbor_tuple->N_status != OLSR_SYM_NEIGH)
						{ //RFC 8.5
							new_neighbor_added = true;
						}
						neighbor_tuple->N_status = OLSR_SYM_NEIGH;
#ifdef ENABLE_ETX
                        if ( (neighbor_tuple->link_quality != current_link_quality) /*|| (neighbor_tuple->link_variance != current_link_variance)*/ || (neighbor_tuple->N_link_quality != neighbor_link_quality) /*|| (neighbor_tuple->N_link_variance != neighbor_link_variance)*/ ){
                            
                            //Update ETX info for sym and asym links
                            neighbor_links_updated = true;
                            neighbor_tuple->link_quality = current_link_quality;
                            neighbor_tuple->N_link_quality = neighbor_link_quality;
                            neighbor_tuple->etx = OLSRMetricETX::calculate_etx(neighbor_tuple->link_quality, neighbor_tuple->N_link_quality);
                            /*double valueToComp = OLSR_METRIC_INFINITY;
                            uint16_t valueToDecomp = OLSRMetricETX::compress(valueToComp);
                            
                            
                            click_chatter("Compressed value of Infinity, %d",valueToDecomp);
                            valueToComp = OLSRMetricETX::decompress(valueToDecomp);
                            click_chatter("Decompressed value of Infinity, %f",valueToComp);
                            
                            valueToDecomp = OLSRMetricETX::compress(100);
                            click_chatter("Compressed value of 100, %d",valueToDecomp);
                            valueToComp = OLSRMetricETX::decompress(valueToDecomp);
                            click_chatter("Decompressed value of 100, %f",valueToComp);
                            
                            valueToDecomp = OLSRMetricETX::compress(11.234);
                            click_chatter("Compressed value of 1111, %d",valueToDecomp);
                            valueToComp = OLSRMetricETX::decompress(valueToDecomp);
                            click_chatter("Decompressed value of 1111, %f",valueToComp);
                            
                            exit(0);*/
                            
                            link_tuple->etx = neighbor_tuple->etx;
                            
                            //Only update if there are real ETX values
                            if (neighbor_tuple->etx < OLSR_METRIC_INFINITY){
                                neighbor_tuple->etx_time = now;
                                link_tuple->etx_time = neighbor_tuple->etx_time;
                                
                                //Handle first case of ever updating ETX link info
                                if ((neighbor_tuple->etx_min == OLSR_METRIC_INFINITY) || (neighbor_tuple->etx_max == OLSR_METRIC_INFINITY)){
                                    neighbor_tuple->etx_min = neighbor_tuple->etx;
                                    neighbor_tuple->etx_min_time = neighbor_tuple->etx_time;
                                    neighbor_tuple->etx_max = neighbor_tuple->etx;
                                    neighbor_tuple->etx_max_time = neighbor_tuple->etx_time;
                                    
                                    link_tuple->etx_min = neighbor_tuple->etx;
                                    link_tuple->etx_min_time = neighbor_tuple->etx_time;
                                    link_tuple->etx_max = neighbor_tuple->etx;
                                    link_tuple->etx_max_time = neighbor_tuple->etx_time;
                                }else{
                                    //This handles all cases greater than the first case updating ETX link info
                                    if (neighbor_tuple->etx < neighbor_tuple->etx_min) {
                                        neighbor_tuple->etx_min = neighbor_tuple->etx;
                                        neighbor_tuple->etx_min_time = neighbor_tuple->etx_time;
                
                                        link_tuple->etx_min = neighbor_tuple->etx;
                                        link_tuple->etx_min_time = neighbor_tuple->etx_time;
                                    }
                                    
                                    if (neighbor_tuple->etx > neighbor_tuple->etx_max) {
                                        neighbor_tuple->etx_max = neighbor_tuple->etx;
                                        neighbor_tuple->etx_max_time = neighbor_tuple->etx_time;
                                        
                                        link_tuple->etx_max = neighbor_tuple->etx;
                                        link_tuple->etx_max_time = neighbor_tuple->etx_time;
                                    }
                                }
                            }
                            
#ifdef ENABLE_VARIANCE
                            //neighbor_tuple->link_variance = current_link_variance;
                            //neighbor_tuple->link_variance = calculate_vtx;
                            //neighbor_tuple->N_link_variance = neighbor_link_variance;
                            
                            //Only update if there are real ETX values
                            if (neighbor_tuple->etx < OLSR_METRIC_INFINITY){
                                neighbor_tuple->variance = OLSRMetricVTX::calculate_vtx(neighbor_tuple->link_quality, neighbor_tuple->N_link_quality);
                                //neighbor_tuple->variance = OLSRMetricVTX::calculate_vtx(neighbor_tuple->link_variance, neighbor_tuple->N_link_variance);
                                link_tuple->variance = neighbor_tuple->variance;
                                neighbor_tuple->var_time = now;
                                link_tuple->var_time = neighbor_tuple->var_time;
                            }
#endif
                        }
                        
#endif
					}
					else
					{
						neighbor_tuple->N_status = OLSR_NOT_NEIGH;
#ifdef ENABLE_ETX
                        neighbor_tuple->link_quality = OLSR_METRIC_INFINITY;
						neighbor_tuple->N_link_quality = OLSR_METRIC_INFINITY;
                        neighbor_tuple->etx = OLSR_METRIC_INFINITY;
                        neighbor_tuple->etx_time = Timestamp::make_msec(0,0);
                        neighbor_tuple->hops = 0;
                        neighbor_tuple->etx_min = OLSR_METRIC_INFINITY;
                        neighbor_tuple->etx_min_time = Timestamp::make_msec(0,0);
                        neighbor_tuple->etx_max = OLSR_METRIC_INFINITY;
                        neighbor_tuple->etx_max_time = Timestamp::make_msec(0,0);
    #ifdef ENABLE_VARIANCE
                        neighbor_tuple->link_variance = OLSR_METRIC_INFINITY;
                        neighbor_tuple->N_link_variance = OLSR_METRIC_INFINITY;
                        neighbor_tuple->variance = OLSR_METRIC_INFINITY;
                        neighbor_tuple->var_time = Timestamp::make_msec(0,0);
    #endif
#endif
					}
                    //click_chatter("*****1COREY ETX: %d",neighbor_tuple->etx);
                    //click_chatter("\n\nMY IP Address: %s",_myMainIp.unparse().c_str());
					//_neighborInfo->print_neighbor_set(); //Added by Corey
                    
					// end 7.1.1 - 2
				}

				//from RFC 8.2.1
				if(update_twohop && (link_info.neigh_type==OLSR_SYM_NEIGH||link_info.neigh_type==OLSR_MPR_NEIGH))
				{
					if ( neighbor_address != _myMainIp )
					{
						IPAddress main_neighbor_address = _interfaceInfo->get_main_address(neighbor_address);
                        twohop_data *two_hop_neighbor = _neighborInfo->find_twohop_neighbor(originator_address, main_neighbor_address);
						if (two_hop_neighbor == 0)
							new_twohop_added = true;
                        else{
                            //If the link quality of the two hop neighbor hasn't changed, no need to request update
                            if ((two_hop_neighbor->etx == neighbor_tuple->etx) /*&& (two_hop_neighbor->variance == neighbor_tuple->variance)*/)
                                update_twohop = false;
                        }
                        
						_neighborInfo->add_twohop_neighbor(originator_address, main_neighbor_address, (now+msg_info.validity_time), neighbor_tuple);

                        //click_chatter("\n\nMY IP Address: %s",_myMainIp.unparse().c_str());
                        //_neighborInfo->print_twohop_set(); //Added by Corey for debugging
					}
				}
				else if (update_twohop && link_info.neigh_type == OLSR_NOT_NEIGH)
				{
					IPAddress main_neighbor_address = _interfaceInfo->get_main_address(neighbor_address);
					_neighborInfo->remove_twohop_neighbor(originator_address, main_neighbor_address);
					twohop_deleted = true; //RFC 8.5
				}	// end 8.2.1

				// from RFC 8.4.1
				//click_chatter ("main address of neighbor: %s, my Main IP %s\n",_interfaceInfo->get_main_address(neighbor_address).unparse().c_str(),_myMainIp.unparse().c_str());
				if ( _interfaceInfo->get_main_address(neighbor_address) == _myMainIp )
				{
					if (link_info.neigh_type == OLSR_MPR_NEIGH )
					{
						mpr_selector_data *mpr_selector = _neighborInfo->find_mpr_selector(originator_address);
						if (mpr_selector == 0)
						{
                            mpr_selector_added = true;
							mpr_selector = _neighborInfo->add_mpr_selector(originator_address, (now + msg_info.validity_time));
                            mpr_selector->etx = neighbor_tuple->etx;
                            mpr_selector->variance = neighbor_tuple->variance;
                            mpr_selector->N_etx = neighbor_tuple->N_link_quality;
                            mpr_selector->N_variance = neighbor_tuple->N_link_variance;
                            mpr_selector->hops = neighbor_tuple->hops;
                            mpr_selector->status = neighbor_tuple->N_status;
                            mpr_selector->willingness = neighbor_tuple->N_willingness;
                            mpr_selector->etx_time = neighbor_tuple->etx_time;
                            mpr_selector->var_time = neighbor_tuple->var_time;
                            mpr_selector->etx_min = neighbor_tuple->etx_min;
                            mpr_selector->etx_min_time = neighbor_tuple->etx_min_time;
                            mpr_selector->etx_max = neighbor_tuple->etx_max;
                            mpr_selector->etx_max_time = neighbor_tuple->etx_max_time;
						}
						else
						{
							mpr_selector->MS_time = now + msg_info.validity_time;
                            
#ifdef ENABLE_ETX
                            //Intent: Technichally if there is a difference in this info, should triger new ansn
                            if ((mpr_selector->etx != neighbor_tuple->etx) /*|| (mpr_selector->variance != neighbor_tuple->variance)*/ ||
                                (mpr_selector->N_etx != neighbor_tuple->N_link_quality) /*|| (mpr_selector->N_variance != neighbor_tuple->N_link_variance)*/ ||
                                (mpr_selector->hops != neighbor_tuple->hops)){
                                
                                mpr_selector_links_updated = true;
                                mpr_selector->etx = neighbor_tuple->etx;
                                mpr_selector->variance = neighbor_tuple->variance;
                                mpr_selector->N_etx = neighbor_tuple->N_link_quality;
                                mpr_selector->N_variance = neighbor_tuple->N_link_quality;
                                mpr_selector->hops = neighbor_tuple->hops;
                                mpr_selector->status = neighbor_tuple->N_status;
                                mpr_selector->willingness = neighbor_tuple->N_willingness;
                                mpr_selector->etx_time = neighbor_tuple->etx_time;
                                mpr_selector->var_time = neighbor_tuple->var_time;
                                mpr_selector->etx_min = neighbor_tuple->etx_min;
                                mpr_selector->etx_min_time = neighbor_tuple->etx_min_time;
                                mpr_selector->etx_max = neighbor_tuple->etx_max;
                                mpr_selector->etx_max_time = neighbor_tuple->etx_max_time;
                            }
#endif
						}
                        
					}
				}//end 8.4.1
			
				interface_address_bytes_left -= size_of_address_field;
				address_offset += size_of_address_field;
			}
			while ( interface_address_bytes_left >= size_of_address_field );
            
            //_neighborInfo->print_neighbor_set(); //Added by Corey for debugging

			link_msg_bytes_left -= link_info.link_msg_size;
			link_msg_offset += link_info.link_msg_size;
			address_offset += olsr_link_hdr_size;
		}
		while (  link_msg_bytes_left >= (olsr_link_hdr_size + size_of_address_field)  );
	}

	//_neighborInfo->print_mpr_selector_set();
	if ( mpr_selector_added || mpr_selector_links_updated)
		_tcGenerator->notify_mpr_selector_changed();  //this includes incrementing ansn; if activated an additional tc message is sent;
	// in a strictly RFC interpretation this should only be done if change is based on link failure
	if (twohop_deleted || new_twohop_added || new_neighbor_added || neighbor_links_updated || update_twohop)
	{
		_neighborInfo->compute_mprset();
		_routingTable->compute_routing_table(false, now);
        if (_fullTopologyTable != 0){
            _fullTopologyTable->compute_routing_table(true, now);
            //_fullTopologyTable->print_routing_table(true);
        }
	}
	output(0).push(packet);
}

void
OLSRProcessHello::compute_htimes(void){
    _neighbor_hold_time_tv = Timestamp::make_msec((int) (_neighbor_hold_time / 1000),(_neighbor_hold_time % 1000));
    //Added by Corey
    _lq_window_time_tv=Timestamp::make_msec ((int) (_hello_period *_lq_window_size / 1000),(_hello_period*_lq_window_size % 1000));
    _hello_time_tv=Timestamp::make_msec ((int) (_hello_period / 1000),(_hello_period % 1000));
}

/// == mvhaen ====================================================================================================
void
OLSRProcessHello::set_period(int period){
    _hello_period = period;
    compute_htimes();
    click_chatter ("OLSRProcessHello - MyIP: %s, _hello_period = %d\n", _myMainIp.unparse().c_str(), _hello_period);
}

void
OLSRProcessHello::set_neighbor_hold_time_tv(int neighbor_hold_time)
{
	_neighbor_hold_time_tv=Timestamp::make_msec ((int) (neighbor_hold_time / 1000),(neighbor_hold_time % 1000));;
	click_chatter ("OLSRProcessHello - MyIP: %s, _neighbor_hold_time_tv = %f\n", _myMainIp.unparse().c_str(), Timestamp(_neighbor_hold_time_tv).doubleval());
}

void
OLSRProcessHello::set_window_size(int window_size)
{
    _lq_window_size = window_size;
    
    click_chatter ( "OLSRProcessHello - MyIP: %s, _lq_window_size = %d \n", _myMainIp.unparse().c_str(), _lq_window_size);
}

int
OLSRProcessHello::set_period_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
    OLSRProcessHello* me = static_cast<OLSRProcessHello*>(e);
    int new_period;
   
    int res = cp_va_kparse(conf, me, errh, "HELLO_PERIOD", cpkP+cpkN, cpInteger, &new_period, cpEnd);
    if ( res < 0 )
        return res;
    me->set_period(new_period);
    return res;
}

int
OLSRProcessHello::set_neighbor_hold_time_tv_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
	OLSRProcessHello* me = static_cast<OLSRProcessHello*>(e);
	int new_nbr_hold_time;
	//int res = cp_va_parse( conf, me, errh, cpInteger, "Neighbor Hold time", &new_nbr_hold_time, 0 );
    int res = cp_va_kparse(conf, me, errh, "NEIGHBOR_HOLD_TIME", cpkP+cpkM, cpInteger, &new_nbr_hold_time, cpEnd);
	if ( res < 0 )
		return res;
	me->set_neighbor_hold_time_tv(new_nbr_hold_time);
	return res;
}

int
OLSRProcessHello::set_window_size_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
    OLSRProcessHello* me = static_cast<OLSRProcessHello*>(e);
    int new_window_size;
    int res = cp_va_kparse( conf, me, errh, "LQ_WINDOW", cpkP+cpkN, cpInteger, &new_window_size, cpEnd);
    if ( res < 0 )
        return res;
    
    me->set_window_size(new_window_size);
    return res;
}

void
OLSRProcessHello::add_handlers()
{
	add_write_handler("set_period", set_period_handler, (void *)0);
    add_write_handler("set_neighbor_hold_time_tv", set_neighbor_hold_time_tv_handler, (void *)0);
    add_write_handler("set_window_size", set_window_size_handler, (void *)0);
}
/// == !mvhaen ===================================================================================================

CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRProcessHello)

