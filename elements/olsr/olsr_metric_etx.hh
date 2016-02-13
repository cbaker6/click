#ifndef OLSRETXMETRIC_HH
#define OLSRETXMETRIC_HH
#include <click/element.hh>
#include "elements/olsr/olsr_metric_generic.hh"
CLICK_DECLS

/*
 * =c
 * OLSRMetricETX(LINKSTAT)
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

const uint32_t OLSR_METRICETX_MULTIPLIER = 1000;

class OLSRMetricETX : public OLSRMetricGeneric {

public:

    OLSRMetricETX();
    ~OLSRMetricETX();
    
  // generic metric methods
    static double calculate_etx(double, double);
    static double sum_etx(double, double);
    static double mean_lq(link_data*, int);
    
    static uint16_t compress(double metric);
    static double decompress(uint16_t metric);

private:
  //LinkStat *_ls;
};

inline uint16_t
OLSRMetricETX::compress(double metric_double)
{
    uint32_t metric;
    uint16_t compressed_metric;
    
    if ((uint32_t)metric_double != OLSR_METRIC_INFINITY){
        metric = (uint32_t)(metric_double*OLSR_METRICETX_MULTIPLIER);
        //click_chatter("***Made it here 2, metric %u",metric);
    }else{
        metric = OLSR_METRIC_INFINITY;
        //click_chatter("***Made it here 3, metric %u, test_value %u, test2 %u ",metric,2^24,2^24-27);
    }
    
    compressed_metric = compress_metric(metric);
    //click_chatter("**Corey metric to compress %u to %u", metric,compressed_metric);
    
    return compressed_metric;
}

inline double
OLSRMetricETX::decompress(uint16_t compressed_metric)
{
    uint32_t metric = decompress_metric(compressed_metric);
    double decompressed_metric;
    
    if (metric != OLSR_METRIC_INFINITY)
        decompressed_metric = ((double)metric / OLSR_METRICETX_MULTIPLIER);
    else
        decompressed_metric = (double)OLSR_METRIC_INFINITY;
    
    //click_chatter("**Corey metric to decompress %u to %u in double form %f", compressed_metric, metric, decompressed_metric);
    return decompressed_metric;
}


inline double
OLSRMetricETX::calculate_etx(double my_lq, double neighbor_lq)
{/*
    int etx_quotient = 0;
    int etx_remainder = 0;
    int denominator = 0;
    
    //Can't divide by 0
    if ((my_lq == 0) || (neighbor_lq ==0))
        return 0;
    else
        denominator = my_lq * neighbor_lq;
    
    etx_quotient = 1000000 / denominator; //incoming LQ's are already *100, this is the same as 100*100/(LQ*100 * NLQ*100)
    etx_remainder = (1000000 % denominator) / denominator; //incoming LQ's are already *100, this is the same as 100/(LQ * NLQ)*10
    
    return (etx_quotient + etx_remainder);*/
    if ((my_lq == OLSR_METRIC_INFINITY) || (neighbor_lq ==OLSR_METRIC_INFINITY))
        return OLSR_METRIC_INFINITY;
    else
        return (1/(my_lq * neighbor_lq));
}

inline double
OLSRMetricETX:: sum_etx(double my_etx, double neighbor_etx)
{
    //If one of the ETX's is 0, the total ETX isn't real
    if ((my_etx == OLSR_METRIC_INFINITY) || (neighbor_etx == OLSR_METRIC_INFINITY))
        return OLSR_METRIC_INFINITY;
    else
        return (my_etx + neighbor_etx);
    /*int etx_quotient = 0;
     int etx_remainder = 0;
     int denominator = 0;
     
     //Can't divide by 0
     if ((my_lq == 0) || (neighbor_lq ==0))
     return 0;
     else
     denominator = my_lq * neighbor_lq;
     
     etx_quotient = 1000000 / denominator;*/ //incoming LQ's are already *100, this is the same as 100*100/(LQ * NLQ)*100
    //etx_remainder = (1000000 % denominator)*100; //incoming LQ's are already *100, this is the same as 100/(LQ * NLQ)*10
    
    //return (etx_quotient /*+ etx_remainder*/);
}

inline double
OLSRMetricETX:: mean_lq(link_data *local_link, int lq_window_size)
{
    if (local_link->L_rec_enough_pkts_to_use_metric){
        /*uint16_t lq_quotient = (local_link->L_num_of_pkts_rec / lq_window_size) * 100;
        uint16_t lq_remainder = (local_link->L_num_of_pkts_rec % lq_window_size) * 10;
        uint16_t lq = lq_quotient + lq_remainder;
        */
        
        //click_chatter("*****COREY Pkts Rcvd: %d LQWindowSize: %d\n",local_link->L_num_of_pkts_rec, lq_window_size);
        return ((double)local_link->L_num_of_pkts_rec / lq_window_size);
    }else{
        return OLSR_METRIC_INFINITY;
    }
}





CLICK_ENDDECLS
#endif
