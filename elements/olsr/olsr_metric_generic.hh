#ifndef OLSRMETRICGENERIC_HH
#define OLSRMETRICGENERIC_HH
#include <click/element.hh>
#include "click_olsr.hh"

CLICK_DECLS

// Public interface to OLSR route metric elements.

class OLSRMetricGeneric {

public:

    OLSRMetricGeneric() { }
    ~OLSRMetricGeneric() { }

    // either a link *or* route metric
    class metric_t {
        bool _good;
        unsigned _val;
    public:
        metric_t() : _good(false), _val(77777) { }
        metric_t(unsigned v, bool g = true) : _good(g), _val(v) { }
        
        unsigned val()  const { return _val;  }
        bool     good() const { return _good; }
    };
    
    static uint16_t compress_metric(uint32_t);
    static uint32_t decompress_metric(uint16_t);
    
    //Prepares metric in what ever way needed then calls compress_metric
    //virtual uint16_t compress(uint32_t);
    
    //Prepares metric in what ever way needed then calls decompress_metric
    //virtual uint32_t decompress(uint16_t compressed_metric);
    
    // Return true iff M1's metric value is `less than' M2's metric
    // value.  `Smaller' metrics are better.  It only makes sense to
    // call this function if m1.good() && m2.good();
    /*virtual bool metric_val_lt(const metric_t &m1, const metric_t &m2);
    
    // Return 1-hop link metric for the link between this node and radio
    // neighbor N.  May be an invalid metric, indicating no such
    // neighbor is known, or that there is not enough data to calculate
    // the metric.  DATA_SENDER should true if this node will be
    // transmitting data to N over the link; if this node is receiving
    // data from N, DATA_SENDER should be false.  This parameter is
    // important for when metrics can be different for each direction of
    // a link.
    virtual metric_t get_link_metric(const EtherAddress &n, bool data_sender);
    
    // Return the route metric resulting from appending the link with
    // metric L to the _end_ of route with metric R.  Either L or R may
    // be invalid, which will result in an invalid combined metric.
    virtual metric_t append_metric(const metric_t &r, const metric_t &l);
    
    // Return the route metric resulting from prepending the link with
    // metric L to the _beginning_ of route with metric R.  Otherwise
    // the same as append_metric().
    virtual metric_t prepend_metric(const metric_t &r, const metric_t &l);
    
    // Most route metrics are commutative.  In this case, they can
    // implement prepend_metric by calling append_metric, since they
    // don't care in which order link metrics are combined.
    
    // XXX I may be excessively zealous here.  Can you think of any
    // non-commutative route metric computation?  What about a
    // non-commutative route metric computation that can be done
    // incrementally, without having all link metrics available at the
    // same time (as this API requires)?
    
    
    // Convert the metric M to a scaled value that fits into one byte;
    // some routing protocols only use one byte for the metric value.
    // The metric's valid bit is ignored.
    virtual unsigned char scale_to_char(const metric_t &m);
    
    // Convert the char value C into a valid, unscaled metric value
    virtual metric_t unscale_from_char(unsigned char c);*/
    
protected:
    //const metric_t _bad_metric; // defined in hopcountmetric.cc

};

//compresses metric as described in OLSRv2 RFC7181 section 6.2
inline uint16_t
OLSRMetricGeneric::compress_metric(uint32_t v)
{
    //Make sure metric is within bounds
    if (v < OLSR_MIMIMUM_METRIC)
        click_chatter("Error in LSRMetricGeneric::compress() metric %u is smaller than allowed minimum %u",v, OLSR_MIMIMUM_METRIC);
    else if (v > OLSR_MAXIMUM_METRIC)
        click_chatter("Error in LSRMetricGeneric::compress() metric %u is larger than allowed maximum %u",v, OLSR_MAXIMUM_METRIC);
    
    uint16_t return_value = 0;
    int b = 0;
    uint32_t v_plus = ( v + OLSR_METRIC_CONSTANT );
    uint32_t v_compare = (uint32_t)( 1 << (b + 9) );
    
    while ( v_plus >= v_compare){
        //click_chatter("v = %u, compv = %u, b= %d", v_plus, v_compare, b);
        b+=1;
        v_compare = (uint64_t)( 1 << (b + 9) );
    }
    
    if ( b == 16 ) {b--;} //If b went out of bounds because of while loop, correct it, a won't need correcting
    
    int a =  (int)(((v)>> b) - OLSR_METRIC_CONSTANT + ((OLSR_METRIC_CONSTANT)>> b) -1);
    //click_chatter("1 For metric v= %u max allowed is %u, a = %d, b = %d",v_plus,OLSR_MAXIMUM_METRIC,a, b);
    int test_value = ((OLSR_METRIC_CONSTANT+1+a)*(1<<b)-OLSR_METRIC_CONSTANT);
    if (test_value < v) a++; //Round up value
    //if ( a == 256 ) {b++; a = 0;} //Make sure values still inbounds
    //click_chatter("2 For metric v= %u, a = %d, b = %d",v,a, b);
    
    if ( ( a <= 255 && a >= 0 ) && ( b <= 15 && b >= 0 ) )
        return_value = ( ( a << 8 ) | (b << 4)); //Bits 0-3 are used TLV of link metric see section 13.3.2
    
    return return_value;
}

//decompresses metric as described in OLSRv2 RFC7181 section 6.2
inline uint32_t
OLSRMetricGeneric::decompress_metric(uint16_t compressed_metric)
{
    //Get a and b values
    int mantessa = (int) (compressed_metric) >> 8;
    int exponent = (int) ((compressed_metric) >> 4) & 0x0f;
    
    return ((OLSR_METRIC_CONSTANT+1+mantessa)*(1<<exponent)-OLSR_METRIC_CONSTANT);
}

CLICK_ENDDECLS
#endif
