//TED 070504: Created

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/udp.h>
#include <clicknet/ether.h>
#include <click/ipaddress.hh>
#include <click/router.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
#include "olsr_tc_generator.hh"
#include "olsr_neighbor_infobase.hh"
#include "olsr_metric_etx.hh"
#include "olsr_metric_vtx.hh"
#include "olsr_metric_delta.hh"
#include "click_olsr.hh"

CLICK_DECLS

OLSRTCGenerator::OLSRTCGenerator()
		: _timer(this)
{
}


OLSRTCGenerator::~OLSRTCGenerator()
{
}


int
OLSRTCGenerator::configure(Vector<String> &conf, ErrorHandler *errh)
{
	bool add_tc_msg=false;
	bool mpr_full_link_state=false;
	bool full_link_state=false;
	
	int res = cp_va_kparse(conf, this, errh,
	                      "TC_SENDING_INTERVAL_MSEC", cpkP+cpkM, cpInteger, &_period,
	                      "TOPOLOGY_HOLDING_TIME_MSEC", cpkP+cpkM, cpInteger, &_top_hold_time,
	                      "NEIGHBOR_INFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_neighborInfo,
	                      "MY_MAIN_IPADDRESS", cpkP+cpkM, cpIPAddress, &_myIP,
                          "CREATE_ROUTES_LOCALLY", cpkP+cpkN, cpBool, &_compute_routing_table_locally,
	                      "ADDITIONAL_TC", cpkN, cpBool,&add_tc_msg,
	                      "MPR_FULL_LINK_STATE", cpkN, cpBool, &mpr_full_link_state,
	                      "FULL_LINK_STATE", cpkN, cpBool, &full_link_state,
	                      cpEnd);
	_additional_TC_msg=add_tc_msg;
	_mpr_full_link_state=mpr_full_link_state;
	_full_link_state=full_link_state;
	if ( res < 0 )
		return res;
	if ( _period <= 0 )
		return errh->error("period must be greater than 0");
	return res;
}


int
OLSRTCGenerator::initialize(ErrorHandler *)
{

	_timer.initialize(this);

	_vtime = compute_vtime();
	_end_of_validity_time = Timestamp::make_msec(0,0);
	_node_is_mpr = false;
	_ansn = 1;
	_last_msg_sent_at = Timestamp::make_msec(0,0);
	return 0;
}

void
OLSRTCGenerator::run_timer(Timer *)
{
    //If not routing locally, no need to generate tc messages
    if (!_compute_routing_table_locally)
        return;
    
	if (_node_is_mpr)
	{
		output(0).push(generate_tc());
		int period=(int)(_period*.95+(random() % (_period/10)));
		//click_chatter ("emitting other tc after %d ms\n",period);
		_timer.reschedule_after_msec(period);
	}
	else
	{
		Packet *p = generate_tc_when_not_mpr();
		if (p != 0)
			output(0).push(p);
	}
}

void
OLSRTCGenerator::set_node_is_mpr(bool value)
{
    
    //If not routing locally, no need to set as mpr
    if (!_compute_routing_table_locally)
        return;
    
	bool node_was_mpr = _node_is_mpr;
	_node_is_mpr = value;
	if (_node_is_mpr)
	{
		_end_of_validity_time = Timestamp::make_msec(0,0);
		int delay=(int) (random() % (_period/20));
		_timer.schedule_after_msec(delay);
		click_chatter ("node %s has become MPR, emitting tc after %d ms\n",_myIP.unparse().c_str(),delay);
	}
	else if (node_was_mpr && !_node_is_mpr)
	{
        Timestamp now = Timestamp::now();
        _end_of_validity_time = Timestamp::make_usec(_period/1000, _period*1000); //period in msec
		_end_of_validity_time += now;
		int delay=(int) (random() % (_period/20));
		_timer.schedule_after_msec(delay);
		click_chatter ("node %s is not mpr emitting tc_not_mpr after %d ms\n",_myIP.unparse().c_str(),delay);
	}

}


Packet *
OLSRTCGenerator::generate_tc()
{
	//   uint64_t cycles=click_get_cycles();
	HashMap<IPAddress, void *> *advertise_set = 0;
	int num_to_advertise = 0;
#ifndef ENABLE_ETX
    int size_of_addr_field = sizeof(in_addr);
#else
    int size_of_addr_field = sizeof(in_address_w_metrics_tc);
#endif
    
	if (_mpr_full_link_state)
	{
		HashMap<IPAddress, void *> *neighbor_set = _neighborInfo->get_neighbor_set();
		if (! neighbor_set->empty())
		{
			for (HashMap<IPAddress, void *>::iterator iter = neighbor_set->begin(); iter != neighbor_set->end(); iter++)
			{
				neighbor_data *nbr = (neighbor_data *) iter.value();
				if (nbr->N_status == OLSR_SYM_NEIGH || nbr->N_status == OLSR_MPR_NEIGH)
				{
					num_to_advertise++;
				}

			}
		}
	}
	else
	{
		advertise_set = _neighborInfo->get_mpr_selector_set();
		num_to_advertise = advertise_set->size();
	}


	int packet_size = sizeof(olsr_pkt_hdr) + sizeof(olsr_msg_hdr) + sizeof(olsr_tc_hdr) + num_to_advertise*size_of_addr_field;
	int headroom = sizeof(click_ether) + sizeof(click_ip) + sizeof(click_udp);
	int tailroom = 0;
	WritablePacket *packet = Packet::make(headroom,0,packet_size, tailroom);
	if ( packet == 0 )
	{
		click_chatter( "in %s: cannot make packet!", name().c_str());
	}
	memset(packet->data(), 0, packet->length());
	//   packet->set_perfctr_anno(cycles);
	olsr_pkt_hdr *pkt_hdr = (olsr_pkt_hdr *) packet->data();
	pkt_hdr->pkt_length = 0; //added in OLSRForward
	pkt_hdr->pkt_seq = 0; //added in OLSRForward

    Timestamp now = Timestamp::now();
	packet->set_timestamp_anno(now);


	olsr_msg_hdr *msg_hdr = (olsr_msg_hdr *) (pkt_hdr + 1);
	msg_hdr->msg_type = OLSR_TC_MESSAGE;
	msg_hdr->vtime = _vtime;
	msg_hdr->msg_size = htons(sizeof(olsr_msg_hdr) + sizeof(olsr_tc_hdr)+ num_to_advertise*size_of_addr_field);
	msg_hdr->originator_address = _myIP.in_addr();
	msg_hdr->ttl = 255;  //TC messages should diffuse into entire network
	msg_hdr->hop_count = 0;
	msg_hdr->msg_seq = 0; //added in OLSRForward element

	olsr_tc_hdr *tc_hdr = (olsr_tc_hdr *) (msg_hdr + 1);
	tc_hdr->ansn = htons( get_ansn() );
	tc_hdr->reserved = 0;

	if ( num_to_advertise != 0 )
	{
		if (_mpr_full_link_state)
		{
#ifndef ENABLE_ETX
			in_addr * address = (in_addr *) (tc_hdr + 1);
#else
            in_address_w_metrics_tc * address = (in_address_w_metrics_tc *) (tc_hdr + 1);
#endif
            
			for (HashMap<IPAddress, void *>::iterator iter = advertise_set->begin(); iter != advertise_set->end(); iter++)
			{
				neighbor_data *nbr = (neighbor_data *) iter.value();
				if (nbr->N_status == OLSR_SYM_NEIGH || nbr->N_status == OLSR_MPR_NEIGH)
				{
#ifndef ENABLE_ETX
					*address = nbr->N_neigh_main_addr.in_addr();
#else
                    in_address_w_metrics_tc addr_w_metrics;
                    addr_w_metrics.address = nbr->N_neigh_main_addr.in_addr();
                    
                    //Have to get current current ETX from neighbor info and update MPR set. This is because MPR's may be selected before link-state info is determined
                    neighbor_data *neighbor_tuple;
                    neighbor_tuple = _neighborInfo->find_neighbor(nbr->N_neigh_main_addr); //Get current neighbor info
                    
#ifdef ENABLE_TOPLOGY_TABLE
                    nbr->N_status = neighbor_tuple->N_status;
                    nbr->N_willingness = neighbor_tuple->N_willingness;
#endif
                    
                    //Update MPR selector information
                    nbr->etx = neighbor_tuple->etx;
                    nbr->variance = neighbor_tuple->variance;
                    
                    //Prepare metrics to be sent in tc message
                    addr_w_metrics.etx = htons(ntohs(OLSRMetricETX::compress(nbr->etx)));
                    addr_w_metrics.dtime =  OLSRMetricDelta::compress(now,neighbor_tuple->etx_time);
    #ifdef ENABLE_VARIANCE
                    
                    addr_w_metrics.variance = htons(ntohs(OLSRMetricVTX::compress(nbr->variance)));
    #endif
                    
                    *address = addr_w_metrics;
#endif
					address++;
				}
			}
		}
		else
		{
#ifndef ENABLE_ETX
			in_addr *address = (in_addr *) (tc_hdr + 1);
#else
            in_address_w_metrics_tc *address = (in_address_w_metrics_tc *) (tc_hdr + 1);
#endif
			for (HashMap<IPAddress, void *>::iterator iter = advertise_set->begin(); iter != advertise_set->end(); iter++)
			{
				mpr_selector_data *mpr_selector = (mpr_selector_data *) iter.value();
#ifndef ENABLE_ETX
				*address = mpr_selector->MS_main_addr.in_addr();
#else
                in_address_w_metrics_tc addr_w_metrics;
                addr_w_metrics.address = mpr_selector->MS_main_addr.in_addr();
                
                //Have to get current current ETX from neighbor info and update MPR set. This is because MPR's may be selected before link-state info is determined
                neighbor_data *neighbor_tuple;
                neighbor_tuple = _neighborInfo->find_neighbor(mpr_selector->MS_main_addr); //Get current neighbor info
                
                //Update MPR selector information
                mpr_selector->etx = neighbor_tuple->etx;
                mpr_selector->variance = neighbor_tuple->variance;
                mpr_selector->hops = neighbor_tuple->hops;
                
                //click_chatter ("*****55555Corey ETX: %d Variance: %d", mpr_selector->etx, mpr_selector->variance);
                
#ifdef ENABLE_TOPLOGY_TABLE
                addr_w_metrics.status = htons(ntohs(mpr_selector->status));
                addr_w_metrics.willingness = htons(ntohs(mpr_selector->willingness));
#endif
                
                addr_w_metrics.etx = htons(ntohs(OLSRMetricETX::compress(mpr_selector->etx)));
                addr_w_metrics.dtime = OLSRMetricDelta::compress(now, mpr_selector->etx_time);
#ifdef ENABLE_VARIANCE
                addr_w_metrics.variance = htons(ntohs(OLSRMetricVTX::compress(mpr_selector->variance)));
#endif
                //click_chatter("*****CLICK %d %d",addr_w_metrics.etx,  addr_w_metrics.variance);
                //mpr_selector->print_mpr_selector_set();
                
                *address = addr_w_metrics;
#endif
				address++;
			}
		}
	}

	/// == mvhaen ====================================================================================================
	// some experimental stuff. maybe finish this later on. Basically has an MPR advertise all its symmetrical neighbors instead of all the MPR selectors
	/*
	num_sym_nbr=0;
	if (! neighbor_set->empty())
	{
		
		for (HashMap<IPAddress, void *>::iterator iter = neighbor_set->begin(); iter; iter++)
		{
			neighbor_data *nbr = (neighbor_data *) iter.value();
			if (nbr->N_status == OLSR_SYM_NEIGH || nbr->N_status == OLSR_MPR_NEIGH) {
				in_addr *address;
				if (num_sym_nbr == 0) {
					address = (in_addr *) (tc_hdr + 1);
				} else {
					address++;
				}
				*address = nbr->N_neigh_main_addr.in_addr();
				num_sym_nbr++;
			}
			
		}
	}
	*/
	/// ==!mvhaen ===================================================================================================



	return packet;
}



Packet *
OLSRTCGenerator::generate_tc_when_not_mpr()
{
	//   uint64_t cycles=click_get_cycles();
    Timestamp now = Timestamp::now();
    
	if (now <= _end_of_validity_time)
	{
		int packet_size = sizeof(olsr_pkt_hdr) + sizeof(olsr_msg_hdr) + sizeof(olsr_tc_hdr) ;
		int headroom = sizeof(click_ether) + sizeof(click_ip) + sizeof(click_udp);
		WritablePacket *packet = Packet::make(headroom,0,packet_size, 0);
		if ( packet == 0 )
		{
			click_chatter( "in %s: cannot make packet!", name().c_str());
		}
		memset(packet->data(), 0, packet->length());
		//   packet->set_perfctr_anno(cycles);
		olsr_pkt_hdr *pkt_hdr =(olsr_pkt_hdr *) packet->data();
		pkt_hdr->pkt_length = 0; //added in OLSRForward
		pkt_hdr->pkt_seq = 0; //added in OLSRForward

		olsr_msg_hdr *msg_hdr = (olsr_msg_hdr *) (pkt_hdr + 1);
		msg_hdr->msg_type = OLSR_TC_MESSAGE;
		msg_hdr->vtime = _vtime;
		msg_hdr->msg_size = htons(sizeof(olsr_msg_hdr) + sizeof(olsr_tc_hdr));
		msg_hdr->originator_address = _myIP.in_addr();
		msg_hdr->ttl = 255;  //TC messages should diffuse into entire network
		msg_hdr->hop_count = 0;
		msg_hdr->msg_seq = 0; //added in OLSRForward element

		olsr_tc_hdr *tc_hdr = (olsr_tc_hdr *) (msg_hdr + 1);
		tc_hdr->ansn = htons( get_ansn() );
		tc_hdr->reserved = 0;

		_timer.reschedule_after_msec(_period);
		return packet;
	}
	return 0;
}


void
OLSRTCGenerator::increment_ansn()
{
	_ansn++;
}

void
OLSRTCGenerator::notify_mpr_selector_changed()
{
	_ansn++;
	if (_additional_TC_msg) _timer.schedule_now();
}


uint16_t
OLSRTCGenerator::get_ansn()
{
	return _ansn;
}


uint8_t
OLSRTCGenerator::compute_vtime()
{
    uint8_t return_value = 0;
    int t = _top_hold_time*1000; //top_hold_time in msec -> t in \B5sec
    //_top_hold_time.tv_usec+_top_hold_time.tv_sec*1000000; //fixpoint -> calculation in \B5sec
    
    int b=0;
    while ((t / OLSR_C_us) >= (1<<(b+1)))
        b++;
    int a=(((16*t/OLSR_C_us)>>b)-16);
    int value=(OLSR_C_us *(16+a)*(1<<b))>>4;
    if (value<t) a++;
    if (a==16) {b++; a=0;}
    
    if ( (a <= 15 && a >= 0) && (b <= 15 && b >= 0) )
        return_value = ((a << 4) | b );
    return return_value;
}

void
OLSRTCGenerator::set_route_locally(bool route_locally)
{
    _compute_routing_table_locally = route_locally;
    click_chatter ( "OLSRTCGenerator - MyIP: %s, _compute_routing_table_locally = %d\n", _myIP.unparse().c_str(), _compute_routing_table_locally );
}

void
OLSRTCGenerator::set_period(int period)
{
    _period = period;
    click_chatter ("OLSRTCGenerator - MyIP: %s, _period = %d\n", _myIP.unparse().c_str(), _period );
}

void
OLSRTCGenerator::set_top_hold(int period)
{
    _top_hold_time = period;
    click_chatter ( "OLSRTCGenerator - MyIP: %s, _top_hold_time = %d\n", _myIP.unparse().c_str(), _top_hold_time );
}

int
OLSRTCGenerator::set_period_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
    OLSRTCGenerator* me = static_cast<OLSRTCGenerator*>(e);
    int new_period;
    int res = cp_va_kparse( conf, me, errh, "TC_SENDING_INTERVAL_MSEC", cpkP+cpkM, cpInteger, &new_period, cpEnd );
    if ( res < 0 )
        return res;
    if ( new_period <= 0 )
        return errh->error( "period must be greater than 0" );
    me->set_period(new_period);
    return res;
}

int
OLSRTCGenerator::set_route_locally_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
    OLSRTCGenerator* me = static_cast<OLSRTCGenerator*>(e);
    bool new_route_locally;
    int res = cp_va_kparse( conf, me, errh, "CREATE_ROUTES_LOCALLY", cpkP+cpkN, cpBool, &new_route_locally, cpEnd);
    if ( res < 0 )
        return res;
    
    me->set_route_locally(new_route_locally);
    return res;
}

int
OLSRTCGenerator::set_top_hold_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
    OLSRTCGenerator* me = static_cast<OLSRTCGenerator*>(e);
    int new_period;
    int res = cp_va_kparse( conf, me, errh, "TOPOLOGY_HOLDING_TIME_MSEC", cpkP+cpkM, cpInteger, &new_period, cpEnd );
    if ( res < 0 )
        return res;
    if ( new_period <= 0 )
        return errh->error( "t_hold must be greater than 0" );
    me->set_top_hold(new_period);
    return res;
}


void
OLSRTCGenerator::add_handlers()
{
    add_write_handler("set_period", set_period_handler, (void *)0);
    add_write_handler("set_top_hold", set_top_hold_handler, (void *)0);
    add_write_handler("set_route_locally", set_route_locally_handler, (void *)0);
}



CLICK_ENDDECLS

EXPORT_ELEMENT(OLSRTCGenerator);


