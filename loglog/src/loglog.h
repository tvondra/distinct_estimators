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
typedef struct LogLogCounterData {
    
    /* length of the structure */
    int32 length;
    
    /* number of bitmaps ('m' in the algorithm) */
	int k;
    int nmaps;
    
    int keysize; /* number of bytes used for bitmap index */
    int mapsize; /* number of bytes used for bitmap element */
    
    /* bitmap used to keep the list of items (uses the very same trick as in
     * the varlena type in include/c.h */
    unsigned char bitmap[1];
    
} LogLogCounterData;

typedef LogLogCounterData * LogLogCounter;

/* creates an optimal bitmap able to count a multiset with the expected
 * cardinality and the given error rate. */
LogLogCounter loglog_create(float error, int ndistinct);
int loglog_get_size(float error, int ndistinct);

/* add element existence */
void loglog_add_element_text(LogLogCounter loglog, const char * element, int elen);
void loglog_add_element_int(LogLogCounter loglog, int element);

/* get an estimate from the loglog counter */
int loglog_estimate(LogLogCounter loglog);

void loglog_reset_internal(LogLogCounter loglog);
