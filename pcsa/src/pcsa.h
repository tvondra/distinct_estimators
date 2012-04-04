#include "postgres.h"

/* This is an implementation of PCSA algorithm as described in the paper
 * "Probalistic Counting Algorithms for Data Base Applications", published
 * by Flajolet and Martin in 1985. Generally it is an advanced version of
 * the 'probabilistic.c' algorithm.
 *
 * There is a small change of the implementation - the original algorithm
 * divides the hash value into two parts (hash MOD m) and (hash DIV m), but
 * we do it a bit differently - we use the first 2 bytes (16 bits) to compute
 * the bitmap index, and the remaining bytes.
 *
 * FIXME The bitmaps are too big (about 15B each, which is way too much as
 * the length in bits determines the possible number of distict values to be
 * tracked - 4B = 32b makes it possible to track up to 2^32 distinct values).
 * So the bitmaps might be much smaller in reality.
 */
typedef struct PCSACounterData {
    
    /* length of the structure */
    int32 length;
    
    /* number of bitmaps */
    int nmaps;
    
    /* number of bytes used for bitmap index */
    int keysize;
    
    /* bitmap used to keep the list of items (uses the very same trick as in
     * the varlena type in include/c.h */
    unsigned char bitmap[1];
    
} PCSACounterData;

typedef PCSACounterData * PCSACounter;

/* creates an optimal bloom filter for the given bitmap size and number of
 * bitmaps (and number of bytes to use for key) */
PCSACounter pcsa_create(int nmaps, int keysize);
int pcsa_get_size(int nmaps, int keysize);

/* add element existence */
void pcsa_add_element_text(PCSACounter pcsa, const char * element, int elen);
void pcsa_add_element_int(PCSACounter pcsa, int element);

/* get an estimate from the probabilistic counter */
int pcsa_estimate(PCSACounter pcsa);

void pcsa_reset_internal(PCSACounter pcsa);
