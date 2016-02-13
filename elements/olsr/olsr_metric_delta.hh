#ifndef OLSRMETRICDELTA_HH
#define OLSRMETRICDELTA_HH
#include <click/element.hh>
#include "elements/olsr/olsr_metric_generic.hh"

CLICK_DECLS

/*
 * =c
 * OLSRMetricDelta(LINKSTAT)
 * =s Grid
 * =io
 * None
 * =d
 *
 * Child class of GridGenericMetric that implements the estimated
 * transmission count (`ETX') metric.
 *
 * LinkStat is this node's LinkStat element, which is needed to obtain
 * the link delivery ratios used to calculate the metric.
 *
 * =a HopcountMetric, LinkStat */

class OLSRMetricDelta : public OLSRMetricGeneric {

public:

    OLSRMetricDelta();
    ~OLSRMetricDelta();

    static Timestamp calculate_delta(Timestamp, Timestamp);
    static Timestamp calculate_delta(Timestamp, Timestamp, Timestamp);
    static Timestamp new_time_after_delta(Timestamp, Timestamp);
    static uint8_t compress(Timestamp now, Timestamp time_metric_received);
    static uint8_t compress(Timestamp now, Timestamp time_metric_received, Timestamp initial_delta);
    static Timestamp decompress(uint8_t);
    
private:
    static Timestamp decompress_metric(int dtime_a, int dtime_b);
    static uint8_t compress_metric(Timestamp now, Timestamp time_metric_received, Timestamp initial_delta);
};

inline Timestamp
OLSRMetricDelta:: calculate_delta(Timestamp now, Timestamp time_metric_received)
{
    return (now - time_metric_received);
}

inline Timestamp
OLSRMetricDelta:: calculate_delta(Timestamp now, Timestamp time_metric_received, Timestamp initial_delta)
{
    return ((now - time_metric_received) + initial_delta);
}

inline Timestamp
OLSRMetricDelta:: new_time_after_delta(Timestamp now, Timestamp initial_delta)
{
    return (now + initial_delta);
}

inline uint8_t
OLSRMetricDelta:: compress(Timestamp now, Timestamp time_metric_received)
{
    return OLSRMetricDelta::compress_metric(now, time_metric_received, Timestamp::make_msec(0,0));
}

inline uint8_t
OLSRMetricDelta:: compress(Timestamp now, Timestamp time_metric_received, Timestamp initial_delta)
{
    return OLSRMetricDelta::compress_metric(now, time_metric_received, initial_delta);
}

//This is used instead of MetricGeneric's compress method
inline uint8_t
OLSRMetricDelta:: compress_metric(Timestamp now, Timestamp time_metric_received, Timestamp initial_delta)
{
    Timestamp delta = OLSRMetricDelta::calculate_delta(now, time_metric_received, initial_delta);
    
    uint8_t return_value = 0;
    int t = delta.msec1(); //top_hold_time in msec -> t in \B5sec
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

inline Timestamp
OLSRMetricDelta:: decompress(uint8_t delta){
    int dtime_a = (int) (delta) >> 4;
    int dtime_b = (int) (delta) & 0x0f;
    
    return OLSRMetricDelta::decompress_metric(dtime_a, dtime_b);
}

//This is used instead of MetricGeneric's decompress method
inline Timestamp
OLSRMetricDelta:: decompress_metric(int dtime_a, int dtime_b)
{
    //calculates validity time as described in OLSR RFC3626 section 3.3.2
    int t;
    Timestamp delta_time;
    
    t=(OLSR_C_us *(16+dtime_a)*(1<<dtime_b))>>4;
    
    delta_time = Timestamp::make_usec(t / 1000000, t % 1000000);
    
    return delta_time;
}


CLICK_ENDDECLS
#endif
