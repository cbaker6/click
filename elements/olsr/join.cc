#include <click/config.h>
#include <click/package.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/string.hh>
#include <click/vector.hh>
#include "join.hh"

CLICK_DECLS

Join::Join()
{

}

Join::~Join()
{
}

int Join::configure(Vector<String> &conf, ErrorHandler *errh)
{
	return Args(conf, this, errh).read_mp("NUM_OF_INTERFACE", _nInterfaces).complete();
}

Packet *
Join::simple_action(Packet *p)
{
   return p;		
}

CLICK_ENDDECLS

EXPORT_ELEMENT(Join)
ELEMENT_MT_SAFE(Join)

