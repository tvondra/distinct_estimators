#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "postgres.h"
#include "libpq/md5.h"

#include "superloglog.h"

#define HASH_LENGTH 16
#define NMAX 1000000000

int superloglog_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes);
int superloglog_get_r(const unsigned char * buffer, int byteFrom, int bytes);
int superloglog_estimate(SuperLogLogCounter loglog);

void superloglog_add_hash(SuperLogLogCounter loglog, const unsigned char * hash);
void superloglog_reset_internal(SuperLogLogCounter loglog);

static int char_comparator(const void * a, const void * b);

/* allocate bitmap with a given length (to store the given number of bitmaps) */
SuperLogLogCounter superloglog_create(float error) {

  float m;
  size_t length = superloglog_get_size(error);

  /* the bitmap is allocated as part of this memory block (-1 as one char is already in) */
  SuperLogLogCounter p = (SuperLogLogCounter)palloc(length);
  
  m = 1.3 / (error * error);
  p->bits  = (int)ceil(log2(m));
  p->m = (int)pow(2, p->bits);
  
  memset(p->data, -1, p->m);
  
  SET_VARSIZE(p, length);
  
  return p;
  
}

int superloglog_get_size(float error) {

  float m = 1.3 / (error * error);
  int bits = (int)ceil(log2(m));

  return offsetof(SuperLogLogCounterData,data) + (int)pow(2, bits);

}

/* searches for the leftmost 1 (aka 'rho' in the algorithm) */
int superloglog_get_min_bit(const unsigned char * buffer, int bitfrom, int nbits) {
  
    int b = 0;
    int byteIdx = 0;
    int bitIdx = 0;
    
    for (b = bitfrom; b < nbits; b++) {
        
        byteIdx = b / 8;
        bitIdx  = b % 8;
        
        if ((buffer[byteIdx] & (0x1 << bitIdx)) != 0)
            return (b - bitfrom + 1);
        
    }
    
    return (nbits-bitfrom) + 1;

}

int superloglog_estimate(SuperLogLogCounter loglog) {
  
    int j;
    float sum = 0;
    
    /* truncation rule */
    int m0 = 0.7 * loglog->m;
    
    /* restriction rule */
    int B = ceil(log(NMAX / loglog->m) / log(2.0) + 3);
    int m2 = 0;
    
    /* sort the 'm' array, to keep only 70% lowest values */
    qsort(loglog->data, loglog->m, sizeof(char), char_comparator);
    
    /* get the estimate for each bitmap */
    for (j = 0; j < m0; j++) {
        if (loglog->data[j] <= B) {
            sum += loglog->data[j];
            m2++;
        }
    }
    
    /* FIXME This uses the same alpha constant as LogLog, not sure how
     * to compute the modified constant :-( */
    return 0.39701 * m0 * powf(2, sum / m0);

}

void superloglog_add_element(SuperLogLogCounter loglog, const char * element, int elen) {
  
    /* get the hash */
    unsigned char hash[HASH_LENGTH];
    
    /* compute the hash */
    pg_md5_binary(element, elen, hash);

    superloglog_add_hash(loglog, hash);
  
}

void superloglog_add_hash(SuperLogLogCounter loglog, const unsigned char * hash) {
  
    /* get the hash */
    unsigned int idx;
    char rho;

    /* which stream is this (keep only the first 'b' bits) */
    memcpy(&idx, hash, sizeof(int));
    idx  = idx >> (32 - loglog->bits);
    
    /* get the min bit (but skip the bits used for stream index) */
    
    /* TODO Originally this was
     * 
     *      rho = hypersuperloglog_get_min_bit(&hash[4], hloglog->b, 64);
     * 
     * but that made the estimates much much worse for some reason. Hmm,
     * maybe the shifts work differently than I thought and so it was
     * somehow coupled to the 'idx'? */
    
    rho = superloglog_get_min_bit(&hash[4], 0, 64); /* 64-bit hash */
    
    /* keep the highest value */
    loglog->data[idx] = (rho > (loglog->data[idx])) ? rho : loglog->data[idx];

}

void superloglog_reset_internal(SuperLogLogCounter loglog) {
    
    memset(loglog->data, 0, loglog->m);

}

static
int char_comparator(const void * a, const void * b) {
    
    char * ac = (char*)a;
    char * bc = (char*)b;
    
    if (ac < bc)
        return -1;
    else if (ac > bc)
        return 1;
    else
        return 0;
}

/* Performs a simple 'copy' of the counter, i.e. allocates a new counter and copies
 * the state from the supplied one. */
SuperLogLogCounter superloglog_copy(SuperLogLogCounter counter) {
    
    size_t length = VARSIZE(counter);
    SuperLogLogCounter copy = (SuperLogLogCounter)palloc(length);
    
    memcpy(copy, counter, length);
    
    return copy;

}

/* Merges the two estimators. Either modifies the first estimator in place (inplace=true),
 * or creates a new copy and returns that (inplace=false). Modification in place is very
 * handy in aggregates, when we really want to modify the aggregate state in place.
 * 
 * Mering is only possible if the counters share the same parameters (number of bins,
 * bin size, ...). If the counters don't match, this throws an ERROR. */
SuperLogLogCounter superloglog_merge(SuperLogLogCounter counter1, SuperLogLogCounter counter2, bool inplace) {

    int i;
    SuperLogLogCounter result;

    /* check compatibility first */
    if (counter1->length != counter2->length)
        elog(ERROR, "sizes of estimators differs (%d != %d)", counter1->length, counter2->length);
    else if (counter1->bits != counter2->bits)
        elog(ERROR, "index size of estimators differs (%d != %d)", counter1->bits, counter2->bits);
    else if (counter1->m != counter2->m)
        elog(ERROR, "bin count of estimators differs (%d != %d)", counter1->m, counter2->m);

    /* shall we create a new estimator, or merge into counter1 */
    if (! inplace)
        result = superloglog_copy(counter1);
    else
        result = counter1;

    /* copy the state of the estimator (keep the higher bin value) */
    for (i = 0; i < result->m; i++)
        result->data[i] = (result->data[i] > counter2->data[i]) ? result->data[i] : counter2->data[i];

    return result;

}