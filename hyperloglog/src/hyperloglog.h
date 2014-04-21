#include "postgres.h"

/* This is an implementation of HyperLogLog algorithm as described in the
 * paper "HyperLogLog: the analysis of near-optimal cardinality estimation
 * algorithm", published by Flajolet, Fusy, Gandouet and Meunier in 2007.
 * Generally it is an improved version of LogLog algorithm with the last
 * step modified, to combine the parts using harmonic means.
 * 
 * TODO The bitmap lengths are encoded in 1B each, which is much more than
 * needed for 32bit hashes (5 bits would be enough, according to the paper).
 * It might be improved (made more space efficient), but IMO it's not really
 * worth the effort and it works with 64bit hashes too which is a good thing
 * if you need to work with large cardinalities. So I'll keep it this for now.
 * 
 * Computing the number of bits based on ndistinct seems rather simple - if
 * 'b' is the number of bits, the estimator should handle 2^(2^b) distinct
 * values. So with 5 bits, each bin can store values between 0 and 31.
 * Apparently 31 is used as a special case 'value too high' so, there are
 * only 30 values, so the bins count ~2^30 values.
 * 
 * By reversing this, we can derive number of bits necessary.
 * 
 * 4 bits -> 2^14 (~16k)
 * 5 bits -> 2^30 (~1e9)
 * 6 bits -> 2^62 (~4e18)
 * 7 bits -> 2^126 ...
 * 
 * So 4 bits seem to low and 6 bits unnecessarily high, and 5 bits is pretty
 * much the best default option.
 * 
 * 
 * TODO Implement merging two estimators (just as with adaptive estimator).
 */
typedef struct HyperLogLogCounterData {
    
    /* length of the structure (varlena) */
    int32 length;
    
    /* Number of counters ('m' in the algorithm) - this is determined depending
     * on the requested error rate - see hyperloglog_create() for details. */
    int b; /* bits for bin index */
    int m; /* m = 2^b */
    
    /* number of bits for a single counter (1B=8bits for now, but may change) */
    int binbits;
    
    /* largest observed 'rho' for each of the 'm' bins (uses the very same trick
     * as in the varlena type in include/c.h */
    char data[1];
    
} HyperLogLogCounterData;

typedef HyperLogLogCounterData * HyperLogLogCounter;

/* creates an optimal bitmap able to count a multiset with the expected
 * cardinality and the given error rate. */
HyperLogLogCounter hyperloglog_create(int64 ndistinct, float error);
int hyperloglog_get_size(int64 ndistinct, float error);

HyperLogLogCounter hyperloglog_copy(HyperLogLogCounter counter);
HyperLogLogCounter hyperloglog_merge(HyperLogLogCounter counter1, HyperLogLogCounter counter2, bool inplace);

/* add element existence */
void hyperloglog_add_element(HyperLogLogCounter hloglog, const char * element, int elen);

/* get an estimate from the hyperloglog counter */
int hyperloglog_estimate(HyperLogLogCounter hloglog);

void hyperloglog_reset_internal(HyperLogLogCounter hloglog);
