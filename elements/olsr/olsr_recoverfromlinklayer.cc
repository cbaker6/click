
#include <click/config.h>
#include <click/confparse.hh>
#include "olsr_recoverfromlinklayer.hh"
#include "click_olsr.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

CLICK_DECLS


OLSRRecoverFromLinkLayer::OLSRRecoverFromLinkLayer()
{
}


OLSRRecoverFromLinkLayer::~OLSRRecoverFromLinkLayer()
{
}


int
OLSRRecoverFromLinkLayer::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _fullTopologyTable = 0;
	if (cp_va_kparse(conf, this, errh,
                     "NEIGHBORINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_neighborInfoBase,
                     "LINKINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_linkInfoBase,
                     "INTERFACEINFOBASE_ELEMENT", cpkP+cpkM, cpElement, &_interfaceInfoBase,
                     "TCGENERATOR_ELEMENT", cpkP+cpkM, cpElement, &_tcGenerator,
                     "ROUTINGTABLE_ELEMENT", cpkP+cpkM, cpElement, &_routingTable,
                     "OLSR_ARP_QUERIER_ELEMENT", cpkP+cpkM, cpElement, &_arpQuerier,
                     "NODES_MAIN_IP_ADDRESS", cpkP+cpkM, cpIPAddress, &_myMainIP,
                     "FULL_TOPOLOGY_TABLE_ELEMENT", cpkN, cpElement, &_fullTopologyTable,
	                cpEnd) < 0)
		return -1;
	return 0;
}


int
OLSRRecoverFromLinkLayer::initialize(ErrorHandler *)
{
	return 0;
}

void
OLSRRecoverFromLinkLayer::push(int, Packet *packet)
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
	// set the gw as dst (for logging purposes)
	packet->set_dst_ip_anno(next_hop_IP);
	// remove the matching link from the link info base
	IPAddress next_hop_main_IP = _interfaceInfoBase->get_main_address(next_hop_IP);
	_linkInfoBase->remove_link(_myMainIP, next_hop_IP);
	// remove the matching neighbor from the neighbor info base if there are no more links
	bool other_interfaces_left = false;
	bool mpr_selector_removed = true;
	for (OLSRLinkInfoBase::LinkSet::iterator iter = _linkInfoBase->get_link_set()->begin(); iter != _linkInfoBase->get_link_set()->end(); iter++)
	{
		link_data *data = (link_data *)iter.value();
		if (_interfaceInfoBase->get_main_address(data->L_neigh_iface_addr) == next_hop_main_IP)
		{
			other_interfaces_left = true;
			break;

		}
	}
	if (!other_interfaces_left)
	{
		_neighborInfoBase->remove_neighbor(next_hop_main_IP);
		if (_neighborInfoBase->find_mpr_selector (next_hop_main_IP))
		{
			_neighborInfoBase->remove_mpr_selector(next_hop_main_IP);
			mpr_selector_removed=true;
		}
	}
	// make sure that the MPRs and Route Table are updated
	if (mpr_selector_removed) _tcGenerator->notify_mpr_selector_changed();
	_neighborInfoBase->compute_mprset();
	_routingTable->compute_routing_table(false,now);
    if (_fullTopologyTable != 0){
        _fullTopologyTable->compute_routing_table(true, now);
        //_fullTopologyTable->print_routing_table(true);
    }
	// now push the packet out through output 0
	output(0).push(packet);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRRecoverFromLinkLayer);

