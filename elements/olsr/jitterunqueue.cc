
#include <click/config.h>
#include "jitterunqueue.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

inline Timestamp
			mk_tval(int sec,int usec)
{
    Timestamp tv = Timestamp::make_usec(sec,usec);
    //I don't think this is needed for timestamp, it should automatically convert usecs to seconds
    /*
	while (1000000 <= tv.usec())
	{
		tv.usec() = tv.usec() - 1000000;
		tv.sec++;
	}*/
	return tv;
}


JitterUnqueue::JitterUnqueue()
		: _task(this)
{
}

JitterUnqueue::~JitterUnqueue()
{
}

int
JitterUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
	int maxdelay =0;
	int mindelay = 0;
	
	int result =  cp_va_kparse(conf, this, errh,
	                          "MAX_DELAY", cpInteger, cpkP+cpkM, &maxdelay,
	                          "MIN_DELAY", cpInteger, cpkN, &mindelay,
	                          cpEnd);
	
    _maxdelay = Timestamp::make_msec ((int) (maxdelay / 1000),(maxdelay % 1000));
	_mindelay = Timestamp::make_msec ((int) (mindelay / 1000),(mindelay % 1000));
	_minmaxdiff = _maxdelay - _mindelay;
	_minmaxdiff_usec = _minmaxdiff.usec() + 1000000*_minmaxdiff.sec();
	return result;
}

int
JitterUnqueue::initialize(ErrorHandler *errh)
{
	ScheduleInfo::initialize_task(this, &_task, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    _expire = Timestamp::make_usec(0,0);
	return 0;
}

bool
JitterUnqueue::run_task(Task *)
{
	// listening for notifications
    Timestamp now = Timestamp::now();
    
	//timeradd(&now, &_delay, &expires);
	bool worked = false;
    
    if (_expire <= now)
	{
		while (Packet *p = input(0).pull())
		{
			output(0).push(p);
			worked = true;

		}
		uint32_t delay_usec = (_minmaxdiff_usec) ? (random() % _minmaxdiff_usec) : 0;
		_expire = now + _mindelay + mk_tval(0,delay_usec);

		if ((!worked) && (!_signal)) // no Packet available
			return false;		// without rescheduling
	}
	_task.fast_reschedule();
	return worked;
}

/// == mvhaen ====================================================================================================
void
JitterUnqueue::set_maxdelay(int maxdelay)
{

	_maxdelay = make_timeval ((int) (maxdelay / 1000),(maxdelay % 1000));
	_minmaxdiff = _maxdelay - _mindelay;
	_minmaxdiff_usec = _minmaxdiff.usec() + 1000000*_minmaxdiff.sec();
}

void
JitterUnqueue::set_mindelay(int mindelay)
{
	_mindelay = make_timeval ((int) (mindelay / 1000),(mindelay % 1000));
	_minmaxdiff = _maxdelay - _mindelay;
	_minmaxdiff_usec = _minmaxdiff.usec() + 1000000*_minmaxdiff.sec();
}

int
JitterUnqueue::set_maxdelay_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
	JitterUnqueue* me = (JitterUnqueue *) e;
	int maxdelay = 0;
	int res =  cp_va_kparse(conf, me, errh,
	                          "MAX_DELAY", cpkP+cpkM, cpInteger, &maxdelay,
	                          cpEnd);
	me->set_maxdelay(maxdelay);
	return res;
}

int
JitterUnqueue::set_mindelay_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
	JitterUnqueue* me = (JitterUnqueue *) e;
	int mindelay = 0;
	int res =  cp_va_kparse(conf, me, errh,
	                          "MIN_DELAY", cpkP+cpkM, cpInteger, &mindelay,
	                          cpEnd);
	me->set_mindelay(mindelay);
	return res;
}

void
JitterUnqueue::add_handlers()
{
	add_write_handler("set_maxdelay", set_maxdelay_handler, (void *)0);
	add_write_handler("set_mindelay", set_mindelay_handler, (void *)0);
}
/// == !mvhaen ===================================================================================================



CLICK_ENDDECLS
EXPORT_ELEMENT(JitterUnqueue)
ELEMENT_MT_SAFE(JitterUnqueue)
