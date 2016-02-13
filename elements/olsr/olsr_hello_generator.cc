//TED 260404 :Created
//Based on grid/hello

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include "olsr_hello_generator.hh"
#include <click/vector.hh>
#include <click/bighashmap.hh>
//#include "ippair.hh"
#include "../wifi/linktable.hh"
#include "olsr_neighbor_infobase.hh"
#include "olsr_link_infobase.hh"
#include "click_olsr.hh"
#include "olsr_packethandle.hh"
#include "olsr_metric_etx.hh"
#include "olsr_metric_vtx.hh"


CLICK_DECLS

OLSRHelloGenerator::OLSRHelloGenerator()
		: _timer( this ), _node_willingness( OLSR_WILLINGNESS )
{
}

OLSRHelloGenerator::~OLSRHelloGenerator()
{
}

int
OLSRHelloGenerator::configure( Vector<String> &conf, ErrorHandler *errh )
{
	int res = cp_va_kparse( conf, this, errh,
	                       "PERIOD_MSEC", cpkP+cpkM, cpInteger, &_period,
	                       "NEIGHBOR_HOLD_TIME", cpkP+cpkM, cpInteger, &_neighbor_hold_time,
	                       "ELEMENT_LINKINFOBASE",  cpkP+cpkM, cpElement, &_linkInfoBase,
	                       "ELEMENT_NEIGHBORINFOBASE", cpkP+cpkM, cpElement, &_neighborInfoBase,
	                       "ELEMENT_INTERFACEINFOBASE", cpkP+cpkM, cpElement, &_interfaceInfoBase,
	                       "ELEMENT_OLSRFORWARD", cpkP+cpkM, cpElement, &_forward,
	                       "INTERFACE_IPADDRESS", cpkP+cpkM, cpIPAddress, &_local_iface_addr,
	                       "MAIN_IP_ADDRESS_OF_NODE", cpkP+cpkM, cpIPAddress, &_myMainIP,
                           "LQ_WINDOW", cpkP+cpkN, cpInteger, &_lq_window_size,
	                       "WILLINGNESS", cpkN, cpInteger, &_node_willingness,
	           		cpEnd);
	if ( res < 0 )
		return res;
	if ( _period <= 0 )
		return errh->error( "period must be greater than 0" );
	return res;
}

int
OLSRHelloGenerator::initialize( ErrorHandler * )
{
	_timer.initialize( this );
	uint32_t start = ( random() % _period );
	click_chatter ( "hello start %d\n", start );
	_timer.schedule_after_msec( start ); // Send OLSR HELLO periodically
	_htime = compute_htime();
	_vtime = compute_vtime();
	click_chatter ( "_neighbor_hold_time = %d | _vtime = %d\n", _neighbor_hold_time, _vtime );
	return 0;
}

void
OLSRHelloGenerator::run_timer(Timer *)
{
	output( 0 ).push( generate_hello() );	
	int period = ( int ) ( _period * .95 + ( random() % ( _period / 10 ) ) );
 	click_chatter ("emitting other hello after %d ms\n",period);
	_timer.reschedule_after_msec( period );
}

Packet *
OLSRHelloGenerator::generate_hello()
{
    //  uint64_t cycles=click_get_cycles();
    Timestamp now = Timestamp::now();
    int number_link_codes = 0;
    int number_addresses = 0;
    HashMap<IPAddress, bool> neighbor_included;
    HashMap<IPPair, void *> *link_set = _linkInfoBase->get_link_set();
    int olsr_link_hdr_size = sizeof(olsr_link_hdr);
    
#ifndef ENABLE_ETX
    int size_of_addr_hdr = sizeof(in_addr);
    HashMap<uint8_t, Vector<IPAddress> > neighbor_interfaces;
    Vector <IPAddress> adv_link_addr;
#else
    int size_of_addr_hdr = sizeof(in_address_w_metrics_hello);
    HashMap<uint8_t, Vector<ipaddress_w_metrics_hello> > neighbor_interfaces;
    Vector <ipaddress_w_metrics_hello> adv_link_addr;
#endif
    
    if ( ! link_set->empty() )
    {
        for ( HashMap<IPPair, void*>::iterator iter = link_set->begin(); iter != link_set->end(); iter++ )
        {
            struct link_data *data;
            data = ( link_data * ) iter.value();
            if ( ( data->L_local_iface_addr == _local_iface_addr ) && ( data->L_time >= now ) )
            {
                uint8_t link_code = get_link_code( data, now );
                //click_chatter ("link with link_code: %d\n",link_code);
                IPAddress main_address = _interfaceInfoBase->get_main_address( data->L_neigh_iface_addr ); //get main address of links remote interface
                neighbor_included.insert ( main_address, true );
#ifndef ENABLE_ETX
                Vector <IPAddress> * neighbor_iface = neighbor_interfaces.findp( link_code );
#else
                Vector <ipaddress_w_metrics_hello> * neighbor_iface = neighbor_interfaces.findp( link_code );
                ipaddress_w_metrics_hello addr_metrics;
                double lq_double = OLSRMetricETX::mean_lq(data, _lq_window_size);//calculate_average_lq(*data);
                
                uint16_t link_quality = OLSRMetricETX::compress(lq_double);
                
#ifdef ENABLE_VARIANCE
                double lq_var_double = OLSRMetricVTX::variance_lq(data, _lq_window_size);//calculate_variance_lq(*data);
                //uint16_t link_variance = OLSRMetricETX::compress(lq_var_double);
#endif
#endif
                
                if ( neighbor_iface == 0 )
                {
                    adv_link_addr.clear();
                    
#ifndef ENABLE_ETX
                    adv_link_addr.push_back( data->L_neigh_iface_addr );
#else
                    addr_metrics.interface_addr = data->L_neigh_iface_addr;
                    addr_metrics.link_quality = link_quality;
#ifdef ENABLE_VARIANCE
                    //addr_metrics.variance = link_variance;
#endif
                    adv_link_addr.push_back( addr_metrics );
#endif
                    
                    neighbor_interfaces.insert( link_code, adv_link_addr );
                    number_link_codes++;
                    number_addresses++;
                }
                else
                {
#ifndef ENABLE_ETX
                    neighbor_iface->push_back( data->L_neigh_iface_addr );
#else
                    addr_metrics.interface_addr = data->L_neigh_iface_addr;
                    addr_metrics.link_quality = link_quality;
#ifdef ENABLE_VARIANCE
                    //addr_metrics.variance = link_variance;
#endif
                    neighbor_iface->push_back( addr_metrics );
#endif
                    number_addresses++;
                }
                //click_chatter("*****COREY111 LQ: %d",link_quality);
            }
        }
    }
    
    HashMap<IPAddress, void *> *neighborSet = _neighborInfoBase->get_neighbor_set();
    
    for ( HashMap<IPAddress, void *> ::iterator iter = neighborSet->begin(); iter != neighborSet->end(); iter++ )
    {
        neighbor_data *neighbor = ( neighbor_data * ) iter.value();
        if ( !neighbor_included.findp( neighbor->N_neigh_main_addr ) )
        {
            uint8_t link_code = OLSR_UNSPEC_LINK;
            if ( _neighborInfoBase->find_mpr( neighbor->N_neigh_main_addr ) != 0 )
                link_code = link_code | ( OLSR_MPR_NEIGH << 2 );
            else
            {
                if ( neighbor->N_status == OLSR_SYM_NEIGH )
                    link_code = link_code | ( OLSR_SYM_NEIGH << 2 );
                else
                    link_code = link_code | ( OLSR_NOT_NEIGH << 2 );
            }
            
#ifndef ENABLE_ETX
            Vector <IPAddress> * neighbor_iface = neighbor_interfaces.findp( link_code );
#else
            Vector <ipaddress_w_metrics_hello> * neighbor_iface = neighbor_interfaces.findp( link_code );
            ipaddress_w_metrics_hello addr_metrics;
            uint16_t link_quality = htons(ntohs(OLSRMetricETX::compress(neighbor->link_quality)));
    
#ifdef ENABLE_VARIANCE
            uint16_t link_variance = htons(ntohs(OLSRMetricVTX::compress(neighbor->link_variance)));
            //click_chatter("*****1COREY VAR: %d",link_quality);
#endif
#endif
            
            if ( neighbor_iface == 0 )
            {
                adv_link_addr.clear();
                
#ifndef ENABLE_ETX
                adv_link_addr.push_back( neighbor->N_neigh_main_addr );
                
#else
                addr_metrics.interface_addr = neighbor->N_neigh_main_addr;
                addr_metrics.link_quality = link_quality;
#ifdef ENABLE_VARIANCE
                //addr_metrics.variance = link_variance;
#endif
                adv_link_addr.push_back( addr_metrics );
#endif
                
                neighbor_interfaces.insert( link_code, adv_link_addr );
                number_link_codes++;
                number_addresses++;
            }
            else
            {
                
#ifndef ENABLE_ETX
                neighbor_iface->push_back( neighbor->N_neigh_main_addr );
#else
                addr_metrics.interface_addr = neighbor->N_neigh_main_addr;
                addr_metrics.link_quality = link_quality;
#ifdef ENABLE_VARIANCE
                //addr_metrics.variance = link_variance;
#endif
                neighbor_iface->push_back( addr_metrics );
#endif
                number_addresses++;
            }
        }
    }
    
    int packet_size = sizeof( olsr_pkt_hdr ) + sizeof( olsr_msg_hdr ) + sizeof( olsr_hello_hdr ) + number_link_codes * olsr_link_hdr_size + number_addresses * size_of_addr_hdr;
    int headroom = sizeof( click_ether ) + sizeof( click_ip ) + sizeof( click_udp );
    int tailroom = 0;
    WritablePacket *packet = Packet::make( headroom, 0, packet_size, tailroom );
    if ( packet == 0 )
    {
        click_chatter( "in %s: cannot make packet!", name().c_str() );
    }
    memset( packet->data(), 0, packet->length() );
    //packet->set_perfctr_anno(cycles);
    
    Timestamp tv = Timestamp::now();
    packet->set_timestamp_anno( tv );
    
    olsr_pkt_hdr *pkt_hdr = ( olsr_pkt_hdr * ) packet->data();
    pkt_hdr->pkt_length = 0;
    pkt_hdr->pkt_seq = 0; //added in OLSRAddPaqSeq
    
    olsr_msg_hdr *msg_hdr = ( olsr_msg_hdr * ) ( pkt_hdr + 1 );
    msg_hdr->msg_type = OLSR_HELLO_MESSAGE;
    msg_hdr->vtime = _vtime;
    msg_hdr->msg_size = htons( sizeof( olsr_msg_hdr ) + sizeof( olsr_hello_hdr ) );
    msg_hdr->originator_address = _myMainIP.in_addr();
    msg_hdr->ttl = 1;  //hello packets MUST NOT be forwarded
    msg_hdr->hop_count = 0;
    
    
    olsr_hello_hdr *hello_hdr = ( olsr_hello_hdr * ) ( msg_hdr + 1 );\
    hello_hdr->reserved = 0;
    hello_hdr->htime = _htime;
    hello_hdr->willingness = _node_willingness;
    
    
    if ( neighbor_interfaces.empty() )
    { //if no neighbors, broadcast willingness
        click_chatter( "OLSRHelloGenerator, no neighbors, broadcasting willingness anyway\n" );
    }
    
    else
    { // there are neighbors, generate link messages
        
        olsr_link_hdr *link_hdr;
        
#ifndef ENABLE_ETX
        in_addr *address = 0;
        
#else
        in_address_w_metrics_hello *address = 0;
#endif
        
        int number_in_neighbor_interfaces = 0; //rather unelegant solution to problem with pointers,
        //avoids overwriting addresses in hello message
        
#ifndef ENABLE_ETX
        for ( HashMap<uint8_t, Vector <IPAddress> >::iterator iter = neighbor_interfaces.begin(); iter != neighbor_interfaces.end(); iter++ )
        {
#else
            for ( HashMap<uint8_t, Vector <ipaddress_w_metrics_hello> >::iterator iter = neighbor_interfaces.begin(); iter != neighbor_interfaces.end(); iter++ )
            {
#endif
                //if (packet->put(sizeof(olsr_link_hdr))==0) click_chatter ("put 1 resulted in 0\n");
                
                if ( number_in_neighbor_interfaces == 0 )
                {
                    link_hdr = ( olsr_link_hdr * ) ( hello_hdr + 1 );
                }
                else
                    link_hdr = ( olsr_link_hdr * ) ( address + 1 );
                
                link_hdr->link_code = iter.key();
                link_hdr->reserved = 0;
                link_hdr->link_msg_size = htons( olsr_link_hdr_size );
#ifndef ENABLE_ETX
                Vector<IPAddress> addr_vector = iter.value();
#else
                Vector<ipaddress_w_metrics_hello> addr_vector = iter.value();
#endif
                int size = addr_vector.size();
                //click_chatter ("adding %d addresses with linkcode %d \n",size, iter.key());
                for ( int i = 0; i < size; i++ )
                {
#ifndef ENABLE_ETX
                    // click_chatter ("\t i=%d: %s\n",i,addr_vector[i].unparse().c_str());
                    if ( i == 0 )
                        address = ( in_addr * ) ( link_hdr + 1 );
                    else
                        address = ( in_addr * ) ( address + 1 );
                    
                    *address = addr_vector[ i ].in_addr();
                    link_hdr->link_msg_size = htons( ntohs( link_hdr->link_msg_size ) + sizeof( in_addr ) );
#else
                    in_address_w_metrics_hello in_addr_w_metrics;
                    // click_chatter ("\t i=%d: %s\n",i,addr_vector[i].unparse().c_str());
                    if ( i == 0 )
                        address = ( in_address_w_metrics_hello * ) ( link_hdr + 1 );
                    else
                        address = ( in_address_w_metrics_hello * ) ( address + 1 );
                    
                    in_addr_w_metrics.address = addr_vector[ i ].interface_addr.in_addr();
                    in_addr_w_metrics.link_quality = addr_vector[ i ].link_quality;
#ifdef ENABLE_VARIANCE
                    //in_addr_w_metrics.variance = addr_vector[ i ].variance;
#endif
                    *address = in_addr_w_metrics;
                    link_hdr->link_msg_size = htons( ntohs( link_hdr->link_msg_size ) + sizeof( in_address_w_metrics_hello ) );
#endif
                }
                
                msg_hdr->msg_size = htons( ntohs( msg_hdr->msg_size ) + ntohs( link_hdr->link_msg_size ) );
                number_in_neighbor_interfaces++;
            }
        }
        
        //click_chatter ("THIS IS COREY0");
        msg_hdr_info msg_info = OLSRPacketHandle::get_msg_hdr_info( packet, sizeof( olsr_pkt_hdr ) );
        //click_chatter ("THIS IS COREY1");
        pkt_hdr->pkt_length = htons( msg_info.msg_size + sizeof( olsr_pkt_hdr ) );
        pkt_hdr->pkt_seq = 0; //added in AddPacketSeq (for each interface)
        msg_hdr->msg_seq = htons ( _forward->get_msg_seq() );	//this also increases the sequence number;
        
        return packet;
}


void OLSRHelloGenerator::notify_mpr_change()
{
	_timer.schedule_now();
}

uint8_t
OLSRHelloGenerator::get_link_code( struct link_data *data, Timestamp now )
{
	uint8_t link_code;
	if ( data->L_SYM_time >= now )
		link_code = OLSR_SYM_LINK;
	else if ( data->L_ASYM_time >= now && data->L_SYM_time < now )
		link_code = OLSR_ASYM_LINK;
	else
		link_code = OLSR_LOST_LINK;

	IPAddress neigh_iface_addr = data->L_neigh_iface_addr.addr();
	IPAddress neigh_main_addr = _interfaceInfoBase->get_main_address( neigh_iface_addr );

	if ( _neighborInfoBase->find_mpr( neigh_main_addr ) != 0 )
		link_code = link_code | ( OLSR_MPR_NEIGH << 2 );
	else
	{
		neighbor_data *neigh_data = _neighborInfoBase->find_neighbor( neigh_main_addr );
		if ( neigh_data != 0 )
		{
			if ( neigh_data->N_status == OLSR_SYM_NEIGH )
				link_code = link_code | ( OLSR_SYM_NEIGH << 2 );
			else
				link_code = link_code | ( OLSR_NOT_NEIGH << 2 );
		}
		else
			click_chatter( "neighbor_type not set myIP: %s\t neighbor main %s\t neigh iface %s\tlinkcode=%d\t\n", _myMainIP.unparse().c_str(), neigh_main_addr.unparse().c_str(), neigh_iface_addr.unparse().c_str(), link_code );
	}
	return link_code;
}


uint8_t
OLSRHelloGenerator::compute_htime()
{
    //_period in milliseconds
    //OLSR_C_us in \B5sec
    uint8_t return_value = 0;
    int t = _period * 1000;
    int b = 0;
    while ( ( t / OLSR_C_us ) >= ( 1 << ( b + 1 ) ) )
        b++;
    int a = ( ( ( 16 * t / OLSR_C_us ) >> b ) - 16 );
    int value = ( OLSR_C_us * ( 16 + a ) * ( 1 << b ) ) >> 4;
    if ( value < t ) a++;
    if ( a == 16 ) {b++; a = 0;}
    if ( ( a <= 15 && a >= 0 ) && ( b <= 15 && b >= 0 ) )
        return_value = ( ( a << 4 ) | b );
    return return_value;
}

uint8_t
OLSRHelloGenerator::compute_vtime()
{
	uint8_t return_value = 0;
	int t = _neighbor_hold_time * 1000; //msec->\B5sec
	//_neighbor_hold_time.tv_usec+_neighbor_hold_time.tv_sec*1000000; //fixpoint -> calculation in \B5sec
	int b = 0;
	while ( ( t / OLSR_C_us ) >= ( 1 << ( b + 1 ) ) )
		b++;
	int a = ( ( ( 16 * t / OLSR_C_us ) >> b ) - 16 );
	int value = ( OLSR_C_us * ( 16 + a ) * ( 1 << b ) ) >> 4;
	if ( value < t ) a++;
if ( a == 16 ) {b++; a = 0;}
	if ( ( a <= 15 && a >= 0 ) && ( b <= 15 && b >= 0 ) )
		return_value = ( ( a << 4 ) | b );
	return return_value;
}

/// == mvhaen ====================================================================================================
void
OLSRHelloGenerator::set_period(int period)
{
	_period = period;
	_htime = compute_htime();
	click_chatter ( "OLSRHelloGenerator - MyIP: %s, _period = %d | _htime = %d\n", _myMainIP.unparse().c_str(), _period, _htime );
}
    
    void
    OLSRHelloGenerator::set_window_size(int window_size)
    {
        _lq_window_size = window_size;
      
        click_chatter ( "OLSRHelloGenerator - MyIP: %s,  _lq_window_size = %d \n", _myMainIP.unparse().c_str(), _lq_window_size);
    }

void
OLSRHelloGenerator::set_neighbor_hold_time(int neighbor_hold_time)
{
	_neighbor_hold_time = neighbor_hold_time;
	_vtime = compute_vtime();
	click_chatter ( "OLSRHelloGenerator - MyIP: %s, _neighbor_hold_time = %d | _vtime = %d\n",_myMainIP.unparse().c_str(), _neighbor_hold_time, _vtime );
}

int
OLSRHelloGenerator::set_period_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
	OLSRHelloGenerator* me = static_cast<OLSRHelloGenerator*>(e);
	int new_period;
	int res = cp_va_kparse( conf, me, errh, "PERIOD_MSEC", cpkP+cpkM, cpInteger, &new_period, cpEnd );
	if ( res < 0 )
		return res;
	if ( new_period <= 0 )
		return errh->error( "period must be greater than 0" );
	me->set_period(new_period);
	return res;
}

int
OLSRHelloGenerator::set_neighbor_hold_time_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
	OLSRHelloGenerator* me = static_cast<OLSRHelloGenerator*>(e);
	int new_nbr_hold_time;
	int res = cp_va_kparse( conf, me, errh, "NEIGHBOR_HOLD_TIME", cpkP+cpkM, cpInteger, &new_nbr_hold_time, cpEnd);	
	if ( res < 0 )
		return res;
    
    me->set_neighbor_hold_time(new_nbr_hold_time);
	return res;
}
    
    int
    OLSRHelloGenerator::set_window_size_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
    {
        OLSRHelloGenerator* me = static_cast<OLSRHelloGenerator*>(e);
        int new_window_size;
        int res = cp_va_kparse( conf, me, errh, "LQ_WINDOW", cpkP+cpkN, cpInteger, &new_window_size, cpEnd);
        if ( res < 0 )
            return res;
        
        me->set_window_size(new_window_size);
        return res;
    }

void
OLSRHelloGenerator::add_handlers()
{
	add_write_handler("set_period", set_period_handler, (void *)0);
	add_write_handler("set_neighbor_hold_time", set_neighbor_hold_time_handler, (void *)0);
    add_write_handler("set_window_size", set_window_size_handler, (void *)0);
}

//COREY ===================================================================================================
  


/// == !mvhaen ===================================================================================================

#include <click/bighashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<IPAddress>;
template class HashMap<IPPair, void *>;
template class HashMap<IPPair, void *>::iterator;
#endif


CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRHelloGenerator);

