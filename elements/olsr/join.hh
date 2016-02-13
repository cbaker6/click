#ifndef JOIN_HH
#define JOIN_HH
#include <click/element.hh>

CLICK_DECLS

class Join : public Element {
  
 public:
  
  Join();
  ~Join();
  
  const char *class_name() const		{ return "Join"; }
  const char *port_count() const  		{ return "1-/1"; }   
  const char *processing() const		{ return AGNOSTIC; } 

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet * simple_action(Packet *);
  

 private: 
  unsigned _nInterfaces;
};


CLICK_ENDDECLS

#endif
