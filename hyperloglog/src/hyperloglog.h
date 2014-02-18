#include "postgres.h"

/* This is an implementation of HyperLogLog algorithm as described in the
 * paper "HyperLogLog: the analysis of near-optimal cardinality estimation
 * algorithm", published by Flajolet, Fusy, Gandouet and Meunier in 2007.
 * Generally it is an improved version of LogLog algorithm with the last
 * step modified, to combine the parts using harmonic means.
 * 
 * TODO The bitmap lengths are encoded in 1B each, which is much more than
 * needed for 32bit hashes (5 bits would be enough). It might be improved,
 * but it's not really worth the effort now and it works with 64bit hashes
 * too which is a good thing. So I'll keep it this for now.
 * 
 * TODO Implement merging two estimators (just as with adaptive estimator).
 */
typedef struct HyperLogLogCounterData {
    
    /* length of the structure */
    int32 length;
    
    /* number of substreams ('m' in the algorithm) */
    int b; /* bits for substream index */
    int m; /* m = 2^b */
    
    /* largest observed rho for each stream (uses the very same trick as in the
     * varlena type in include/c.h */
    char data[1];
    
} HyperLogLogCounterData;

typedef HyperLogLogCounterData * HyperLogLogCounter;

/* creates an optimal bitmap able to count a multiset with the expected
 * cardinality and the given error rate. */
HyperLogLogCounter hyperloglog_create(float error);
int hyperloglog_get_size(float error);

/* add element existence */
void hyperloglog_add_element_text(HyperLogLogCounter hloglog, const char * element, int elen);
void hyperloglog_add_element_int(HyperLogLogCounter hloglog, int element);

/* get an estimate from the hyperloglog counter */
int hyperloglog_estimate(HyperLogLogCounter hloglog);

void hyperloglog_reset_internal(HyperLogLogCounter hloglog);
