#include "postgres.h"

/* This is an implementation of LogLog algorithm as described in the paper
 * "LogLog counting of large cardinalities", published by Durand and Flajolet
 * in 2003.
 *
 * FIXME The bitmap lengths are aligned to bytes, so they are a lot longer than
 * needed usually.
 * 
 * TODO Implement super-loglog version of the algorithm, that performs truncation
 * of the bitmap values (see p. 11 of the paper).
 * 
 * TODO Implement merging two estimators (just as with adaptive estimator).
 */
typedef struct SuperLogLogCounterData {
    
    /* length of the structure */
    int32 length;
    
    /* number of the slots (and addressing bits) */
    int bits;
    int m; /* m = 2^bits*/
    
    /* bitmap used to keep the list of items (uses the very same trick as in
     * the varlena type in include/c.h */
    char data[1];
    
} SuperLogLogCounterData;

typedef SuperLogLogCounterData * SuperLogLogCounter;

/* creates an optimal bitmap able to count a multiset with the expected
 * cardinality and the given error rate. */
SuperLogLogCounter superloglog_create(float error);
int superloglog_get_size(float error);

/* add element existence */
void superloglog_add_element(SuperLogLogCounter sloglog, const char * element, int elen);

/* get an estimate from the loglog counter */
int superloglog_estimate(SuperLogLogCounter loglog);

void superloglog_reset_internal(SuperLogLogCounter loglog);

SuperLogLogCounter superloglog_copy(SuperLogLogCounter counter);
SuperLogLogCounter superloglog_merge(SuperLogLogCounter counter1, SuperLogLogCounter counter2, bool inplace);