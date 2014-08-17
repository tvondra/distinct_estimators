#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "postgres.h"
#include "libpq/md5.h"

#include "hyperloglog.h"

/* we're using md5, which produces 16B (128-bit) values */
#define HASH_LENGTH 16

/* Alpha constants, for various numbers of 'b'.
 * 
 * According to hyperloglog_create the 'b' values are between 4 and 16,
 * so the array has 16 non-zero items matching indexes 4, 5, ..., 16.
 * This makes it very easy to access the constants.
 */
static float alpha[] = {0, 0, 0, 0, 0.673, 0.697, 0.709, 0.7153, 0.7183, 0.7198, 0.7205,
                        0.7209, 0.7211, 0.7212, 0.7213, 0.7213, 0.7213};

int hyperloglog_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes);
int hyperloglog_get_r(const unsigned char * buffer, int byteFrom, int bytes);
int hyperloglog_estimate(HyperLogLogCounter hloglog);

void hyperloglog_add_hash(HyperLogLogCounter hloglog, const unsigned char * hash);
void hyperloglog_reset_internal(HyperLogLogCounter hloglog);

/* Allocate HLL estimator that can handle the desired cartinality and precision.
 *
 * TODO The ndistinct is not currently used to determine size of the bin (number of
 * bits used to store the counter) - it's always 1B=8bits, for now, to make it
 * easier to work with. See the header file (hyperloglog.h) for discussion of this.
 * 
 * parameters:
 *      ndistinct   - cardinality the estimator should handle
 *      error       - requested error rate (0 - 1, where 0 means 'exact')
 * 
 * returns:
 *      instance of HLL estimator (throws ERROR in case of failure)
 */
HyperLogLogCounter hyperloglog_create(int64 ndistinct, float error) {

    float m;
    size_t length = hyperloglog_get_size(ndistinct, error);

    /* the bitmap is allocated as part of this memory block (-1 as one bin is already in) */
    HyperLogLogCounter p = (HyperLogLogCounter)palloc(length);

    /* target error rate needs to be between 0 and 1 */
    if (error <= 0 || error >= 1)
        elog(ERROR, "invalid error rate requested - only values in (0,1) allowed");

    /* what is the minimum number of bins to achieve the requested error rate? we'll
     * increase this to the nearest power of two later */
    m = 1.04 / (error * error);

    /* so how many bits do we need to index the bins (nearest power of two) */
    p->b = (int)ceil(log2(m));

    /* TODO Is there actually a good reason to limit the number precision to 16 bits? We're
    * using MD5, so we have 128 bits available ... It'll require more memory - 16 bits is 65k
    * bins, requiring 65kB of  memory, which indeed is a lot. But why not to allow that if
    * that's what was requested? */

    if (p->b < 4)   /* we want at least 2^4 (=16) bins */
        p->b = 4;
    else if (p->b > 16)
        elog(ERROR, "number of index bits exceeds 16 (requested %d)", p->b);

    p->m= (int)pow(2, p->b);
    memset(p->data, 0, p->m);

    /* use 1B for a counter by default */
    p->binbits = 8;

    SET_VARSIZE(p, length);

    return p;

}

/* Performs a simple 'copy' of the counter, i.e. allocates a new counter and copies
 * the state from the supplied one. */
HyperLogLogCounter hyperloglog_copy(HyperLogLogCounter counter) {
    
    size_t length = VARSIZE(counter);
    HyperLogLogCounter copy = (HyperLogLogCounter)palloc(length);
    
    memcpy(copy, counter, length);
    
    return copy;

}

/* Merges the two estimators. Either modifies the first estimator in place (inplace=true),
 * or creates a new copy and returns that (inplace=false). Modification in place is very
 * handy in aggregates, when we really want to modify the aggregate state in place.
 * 
 * Mering is only possible if the counters share the same parameters (number of bins,
 * bin size, ...). If the counters don't match, this throws an ERROR. */
HyperLogLogCounter hyperloglog_merge(HyperLogLogCounter counter1, HyperLogLogCounter counter2, bool inplace) {

    int i;
    HyperLogLogCounter result;

    /* check compatibility first */
    if (counter1->length != counter2->length)
        elog(ERROR, "sizes of estimators differs (%d != %d)", counter1->length, counter2->length);
    else if (counter1->b != counter2->b)
        elog(ERROR, "index size of estimators differs (%d != %d)", counter1->b, counter2->b);
    else if (counter1->m != counter2->m)
        elog(ERROR, "bin count of estimators differs (%d != %d)", counter1->m, counter2->m);
    else if (counter1->binbits != counter2->binbits)
        elog(ERROR, "bin size of estimators differs (%d != %d)", counter1->binbits, counter2->binbits);

    /* shall we create a new estimator, or merge into counter1 */
    if (! inplace)
        result = hyperloglog_copy(counter1);
    else
        result = counter1;

    /* copy the state of the estimator */
    for (i = 0; i < result->m; i++)
        result->data[i] = (result->data[i] > counter2->data[i]) ? result->data[i] : counter2->data[i];

    return result;

}


/* Computes size of the structure, depending on the requested error rate.
 * 
 * TODO The ndistinct is not currently used to determine size of the bin.
 */
int hyperloglog_get_size(int64 ndistinct, float error) {

  int b;
  float m;

  if (error <= 0 || error >= 1)
      elog(ERROR, "invalid error rate requested");

  m = 1.04 / (error * error);
  b = (int)ceil(log2(m));

  if (b < 4)
      b = 4;
  else if (b > 16)
      elog(ERROR, "number of bits in HyperLogLog exceeds 16");

  return offsetof(HyperLogLogCounterData,data) + (int)pow(2, b);

}

/* searches for the leftmost 1 (aka 'rho' in the algorithm) */
int hyperloglog_get_min_bit(const unsigned char * buffer, int bitfrom, int nbits) {

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

/*
 * Computes the HLL estimate, as described in the paper.
 * 
 * In short it does these steps:
 * 
 * 1) sums the data in counters (1/2^m[i])
 * 2) computes the raw estimate E
 * 3) corrects the estimate for low/high values
 * 
 */
int hyperloglog_estimate(HyperLogLogCounter hloglog) {

    double sum = 0, E = 0;
    int j;

    /* compute the sum for the indicator function */
    for (j = 0; j < hloglog->m; j++)
        sum += (1.0 / pow(2, hloglog->data[j]));

    /* and finally the estimate itself */
    E = alpha[hloglog->b] * pow(hloglog->m, 2) / sum;

    if (E <= (5.0 * hloglog->m / 2)) {

        int V = 0;
        for (j = 0; j < hloglog->m; j++)
            if (hloglog->data[j] == 0)
                V += 1;

        if (V != 0)
            E = hloglog->m * log(hloglog->m / (float)V);

    } else if (E > pow(2, 32) / 30) {

        E = -pow(2,32) * log(1 - E / pow(2,32));

    }

    return (int)E;

}


void hyperloglog_add_element(HyperLogLogCounter hloglog, const char * element, int elen) {

    /* get the hash */
    unsigned char hash[HASH_LENGTH];

    /* compute the hash using the salt */
    pg_md5_binary(element, elen, hash);

    /* add the hash to the estimator */
    hyperloglog_add_hash(hloglog, hash);

}

/*
 * Adds a 32-bit hash (computed from the actual item) into the counter. First it computes
 * the counter index from the first 32 bits of the hash, then uses the remaining data to
 * compute the value of the counter (aka 'rho').
 * 
 * This assumes the hash is at least 12B of data (96b = 32b + 64b), as it supports 1B
 * counters / rho values returning values up to 64 (which could fit into less than 8b,
 * but whatever).
 * 
 * However we're internally using MD5, which produces 128b (16B), so this is OK. Using
 * a shorter hash values (say CRC32 producing 32b values) would be possible too, but it
 * would require some tweaks.
 * 
 * Important note - it's essential to keep the counter index / rho independent, otherwise
 * the estimates will be much worse than the requested error rate. For example the 'rho'
 * was originally computed like this:
 * 
 *      rho = hyperloglog_get_min_bit(&hash[4], hloglog->b, 64);
 * 
 * which is not independent with the index because of how the shifts work. So the current
 * implementation simply uses the next 4B for rho.
 */
void hyperloglog_add_hash(HyperLogLogCounter hloglog, const unsigned char * hash) {

    /* get the hash */
    unsigned int idx;
    char rho;

    /* which stream is this (keep only the first 'b' bits) */
    memcpy(&idx, hash, sizeof(int));
    idx  = idx >> (32 - hloglog->b);

    /* get the min bit (but skip the bits used for stream index) */

    /* needs to be independent from 'idx' */
    rho = hyperloglog_get_min_bit(&hash[4], 0, 64); /* 64-bit hash */

    /* keep the highest value */
    hloglog->data[idx] = (rho > (hloglog->data[idx])) ? rho : hloglog->data[idx];

}

/* Just reset the counter (set all the counters to 0). */
void hyperloglog_reset_internal(HyperLogLogCounter hloglog) {

    memset(hloglog->data, 0, hloglog->m);

}
