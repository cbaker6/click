
#include <click/config.h>
#include <click/confparse.hh>
#include "olsr_rarp.hh"
#include "click_olsr.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

CLICK_DECLS


OLSRRARP::OLSRRARP()
{
}


OLSRRARP::~OLSRRARP()
{
}


int
OLSRRARP::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh,
	                "OLSR_ARP_QUERIER_ELEMENT", cpkP+cpkM, cpElement, &_arpQuerier,
	                "NODES_MAIN_IP_ADDRESS", cpkP+cpkM, cpIPAddress, &_myMainIP,
	                cpEnd) < 0)
		return -1;
	return 0;
}


int
OLSRRARP::initialize(ErrorHandler *)
{
	return 0;
}

void
OLSRRARP::push(int, Packet *packet)
{
    Timestamp now = Timestamp::now();

	EtherAddress ether_addr;
	memcpy(ether_addr.data(), packet->data(), 6);

	// do reverse ARP
	IPAddress next_hop_IP = _arpQuerier->lookup_mac(ether_addr);

	if (next_hop_IP == IPAddress("0.0.0.0"))
	{
		output(1).push(packet);		//this case should not occur that much
		return;
	}

	click_chatter("%f | %s | %s | reverse lookup succeeded: the original next hop was %s", Timestamp(now).doubleval(), _myMainIP.unparse().c_str(), __FUNCTION__, next_hop_IP.s().c_str());
	// set the gw as dst
	packet->set_dst_ip_anno(next_hop_IP);

	output(0).push(packet);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRRARP);

