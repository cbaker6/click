#ifndef OLSRMETRICVTX_HH
#define OLSRMETRICVTX_HH
#include <click/element.hh>
#include "elements/olsr/olsr_metric_generic.hh"
//#include "elements/olsr/olsr_metric_etx.hh"
CLICK_DECLS

/*
 * =c
 * OLSRMetricVTX(LINKSTAT)
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


const uint32_t OLSR_METRICVTX_MULTIPLIER = 10000000;

class OLSRMetricVTX : public OLSRMetricGeneric {

public:

    OLSRMetricVTX();
    ~OLSRMetricVTX();

  // generic metric methods
    static double calculate_vtx(double, double);
    static double calculate_covtx(double, double);
    static double variance_lq(link_data*, int);
    //static uint16_t vtx(uint16_t,uint16_t); //Change this, not needed
    //static double vtx_lq(link_data*, int, uint16_t);
    
    static uint16_t compress(double metric);
    static double decompress(uint16_t metric);
  
private:
  //LinkStat *_ls;
};

inline uint16_t
OLSRMetricVTX::compress(double metric_double)
{
    uint32_t metric;
    uint16_t compressed_metric;
    
    if ((uint32_t)metric_double != OLSR_METRIC_INFINITY){
        metric = (uint32_t)(metric_double*OLSR_METRICVTX_MULTIPLIER);
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
OLSRMetricVTX::decompress(uint16_t compressed_metric)
{
    uint32_t metric = decompress_metric(compressed_metric);
    double decompressed_metric;
    
    if (metric != OLSR_METRIC_INFINITY)
        decompressed_metric = ((double)metric / OLSR_METRICVTX_MULTIPLIER);
    else
        decompressed_metric = (double)OLSR_METRIC_INFINITY;
    
    //click_chatter("**Corey metric to decompress %u to %u in double form %f", compressed_metric, metric, decompressed_metric);
    return decompressed_metric;
}


inline double
OLSRMetricVTX:: calculate_vtx(double my_p, double neighbor_p)
{
    /*int etx_quotient = 0;
    int etx_remainder = 0;
    int denominator = 0;
    int numerator = 0;
    
    //Can't divide by 0
    if ((my_etx == 0) || (neighbor_etx == 0))
        return 0;
    else
        denominator = ((etx_quotient * neighbor_etx)/10000)^2;
    
    numerator = (100);
    
    etx_quotient = 1000000 / denominator; //incoming LQ's are already *100, this is the same as 100*100/(LQ*100 * NLQ*100)
    etx_remainder = (1000000 % denominator) / denominator; //incoming LQ's are already *100, this is the same as 100/(LQ * NLQ)*10
    
    return (etx_quotient + etx_remainder);
     //return (my_vtx + neighbor_vtx);
    */
    
    double p = (my_p * neighbor_p); ///10000;
    double vtx  = (1 - p)/(p * p);
    
    //click_chatter("*****COREY MyP: %f NeighborP: %f p: %f VTX: %f", my_p, neighbor_p, p, vtx);
    
    return vtx;
    
    
}

inline double
OLSRMetricVTX:: calculate_covtx(double my_p, double neighbor_p)
{
    /*int etx_quotient = 0;
     int etx_remainder = 0;
     int denominator = 0;
     int numerator = 0;
     
     //Can't divide by 0
     if ((my_etx == 0) || (neighbor_etx == 0))
     return 0;
     else
     denominator = ((etx_quotient * neighbor_etx)/10000)^2;
     
     numerator = (100);
     
     etx_quotient = 1000000 / denominator; //incoming LQ's are already *100, this is the same as 100*100/(LQ*100 * NLQ*100)
     etx_remainder = (1000000 % denominator) / denominator; //incoming LQ's are already *100, this is the same as 100/(LQ * NLQ)*10
     
     return (etx_quotient + etx_remainder);
     //return (my_vtx + neighbor_vtx);
     */
    
    double p1 = (my_p * neighbor_p); ///10000;
    double vtx1  = (1 - p1)/(p1 * p1);
    
    double p2 = (neighbor_p); ///10000;
    double vtx2  = (1 - p2)/(p2 * p2);
    
    //click_chatter("*****COREY MyP: %f NeighborP: %f p: %f VTX: %f", my_p, neighbor_p, p, vtx);
    
    return vtx1 + vtx2;
    
    
}

//Calcultes the variance assuming Tx is a Bernoulli trial
inline double
OLSRMetricVTX:: variance_lq(link_data *local_link, int lq_window_size)
{
    
    if (local_link->L_rec_enough_pkts_to_use_metric){
        uint16_t mean = OLSRMetricETX::mean_lq(local_link, lq_window_size);
        uint16_t numerator = ((local_link->L_num_of_pkts_rec * (100 - mean)^2) + ((lq_window_size - local_link->L_num_of_pkts_rec) * (mean)^2) / 100);
        uint16_t denominator = (lq_window_size);
        uint16_t lq_quotient = (numerator / denominator);
        //uint16_t lq_remainder = (numerator % denominator);
        uint16_t lq = lq_quotient; //+ lq_remainder;
        
        //click_chatter("*****COREY Pkts Rcvd: %d VARIANCE: %d MEAN: %d NUMER: %d DENOM: %d\n",local_link->L_num_of_pkts_rec, lq, mean, numerator, denominator);
        return lq;
    }else{
        return OLSR_METRIC_INFINITY;
    }
    
    
}

//Calcultes the variance assuming a geometric series
/*inline uint16_t
OLSRMetricVTX:: vtx_lq(link_data *local_link, int lq_window_size, double neighbor_p)
{
    
    if (local_link->L_rec_enough_pkts_to_use_metric){
        uint16_t my_p = OLSRMetricETX::mean_lq(local_link, lq_window_size);
        uint16_t p = my_p * neighbor_p;
        uint16_t p2 = p / lq_window_size;
        
        uint16_t vtx  = (100 - p2)/(p2^2);
        
        click_chatter("*****COREY Pkts Rcvd: %d VTX: %d p: %d p2: %d",local_link->L_num_of_pkts_rec, vtx, p, p2);
        
        return vtx;
        
    }else{
        return 0;
    }
    
}*/





CLICK_ENDDECLS
#endif
