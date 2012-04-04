#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "bitmap.h"
#include "postgres.h"

#include "libpq/md5.h"

#define HASH_LENGTH 16

void bc_hash_text(unsigned char * buffer, const char * element, int length);
void bc_hash_int(unsigned char * buffer, int element);

unsigned int bc_get_bits(const unsigned char * src, int from, int length);

float bc_pk(BitmapCounter bc, int k);
float bc_p(BitmapCounter bc);
float bc_q(BitmapCounter bc, int l);
float bc_t(BitmapCounter bc, int l);

void bc_add_hash(BitmapCounter bc, const unsigned char * hash, int length);
int bc_estimate(BitmapCounter bc);

/* Create the bitmap counter - compute the optimal bitmap length, etc.  */
BitmapCounter bc_init(float error, int ndistinct) {

    /* compute the number of bits (see the paper "distinct counting with self-learning bitmap" page 3) */
    float m = log(1 + 2*ndistinct*powf(error,2)) / log(1 + 2*powf(error,2)/(1 - powf(error,2)));

    /* how many bits do we need for index (size of the 'c') */
    int bits = ceil(log(m)/log(2));
    
    /* size of the bitmap */
    int nbits = (0x1 << bits);

    /* size of the bitmap (in bytes) */
    int bitmapSize = nbits/sizeof(char);
    
    int i = 0;

    BitmapCounter b = (BitmapCounter)palloc(sizeof(BitmapCounterData) + bitmapSize - 1);
    
    for (i = 0; i < bitmapSize; i++) {
        b->bitmap[i] = 0;
    }

    /* total length of the bitmap */
    SET_VARSIZE(b, sizeof(BitmapCounterData) + bitmapSize - VARHDRSZ);
    
    /* save the parameters (error rate, number of distinct values) */
    b->error = error;
    b->ndistinct = ndistinct;
    b->level = 0;
    
    /* helper (used to compute a lot of other values) */
    b->r = 1 - 2 * powf(b->error,2) / (1 + powf(b->error,2));
    
    /* how many bits do we need for index (size of the 'c') */
    b->cbits = bits;
    
    /* FIXME 'd' should be computed from O(log(ndistinct)), but I'm not sure how */
    b->dbits = bits; /* temporary fix - use the same number of bits as for 'c' */
    
    /* size of the bitmap (used to tranck*/
    b->nbits = nbits;

    return b;
  
}

/* Computes an MD5 hash of the input value (with a given length). */
void bc_hash_text(unsigned char * buffer, const char * element, int length) {
    pg_md5_binary(element, length, buffer);
}

/* Computes an MD5 hash of the input value (with a given length). */
void bc_hash_int(unsigned char * buffer, int element) {
    pg_md5_binary(&element, sizeof(int), buffer);
}

/* Copies the given number of bits from the array to an unsigned integer */
unsigned int bc_get_bits(const unsigned char * src, int from, int length) {
  
    /* all bits are 0 */
    unsigned int index = 0;
    
    /* get the starting point */
    int srcByteIdx = from / 8; /* position of the byte */
    int srcBitIdx = from % 8; /* position within a byte (0-7), number of shifts */
    
    /* position in the target int */
    int dstByteIdx = 0;
    int dstBitIdx = 0;

    int i = 0;
    
    /* max index is 2^32 (512MB bitmap, which is way too bit anyway) */
    if (length > 32) {
        elog(ERROR, "Max index size is 2^32 (512MB).");
        return 0;
    }
    
    while (i < length) {
        
        /* check the bit in the array, and set it in the int if 1 */
        if (src[srcByteIdx] & (0x1 << srcBitIdx)) {
            index = index | (0x1 << (dstByteIdx * 8 + dstBitIdx));
        }
        
        /* move src indices */
        ++srcBitIdx;
        if (srcBitIdx == 8) {
            srcBitIdx = 0;
            ++srcByteIdx;
        }
        
        /* move dst indices */
        ++dstBitIdx;
        if (dstBitIdx == 8) {
            dstBitIdx = 0;
            ++dstByteIdx;
        }
        
        ++i; /* skip to the next bit */
    }
    
    return index;

}

float bc_pk(BitmapCounter bc, int k) {
    return (float)bc->nbits * (1 + powf(bc->error,2)) * powf(bc->r,k) / (bc->nbits + 1 - k);
}

float bc_p(BitmapCounter bc) {
    return bc_pk(bc, bc->level);
}

float bc_q(BitmapCounter bc, int l) {
    return ((float)(bc->nbits - l + 1) * bc_pk(bc, l)) / bc->nbits;
}

float bc_t(BitmapCounter bc, int l) {
  
    float tb = 0;
    int i;
  
    for (i = 1; i <= l; i++) {
        tb += 1 / bc_q(bc, i);
    }
  
    return tb;
  
}

int bc_estimate(BitmapCounter bc) {
  
    float t1 = bc_t(bc, bc->level);
    float t2 = bc_t(bc, bc->level+1);
  
    if ((bc->nbits > bc->level) && (t2 - t1 > 1.5)) {
        return (int)round(2 * t1 * t2 / (t1 + t2));
    } else {
        return (int)t1;
    }
  
}

void bc_add_hash(BitmapCounter bc, const unsigned char * hash, int length) {

    /* get the bitmap index 'c' and the 'u' value */
    unsigned int index = bc_get_bits(hash,         0, bc->cbits); /* first 'c' bits */
    unsigned int u     = bc_get_bits(hash, bc->cbits, bc->dbits); /* next 'd' bits */
    
    int bitIdx = index % 8; /* position within a byte */
    int byteIdx = index / 8; /* position of the byte */
    
    /* if the bitmap index is already set, do nothing, else try */
    if (! (bc->bitmap[byteIdx] & (0x1 << bitIdx))) {
        
        float prob = bc_p(bc); /* get the p(k) probability */
        
        if (((float)u/powf(2,bc->dbits)) < prob) {
            bc->bitmap[byteIdx] = bc->bitmap[byteIdx] | (0x1 << bitIdx);
            bc->level += 1;
        }
        
    }
  
}

void bc_add_item_text(BitmapCounter bc, const char * item, int length) {

    /* allocate buffer (for the hash result) */
    unsigned char buffer[HASH_LENGTH];
    
    /* get the hash */
    bc_hash_text(buffer, item, length);
    
    bc_add_hash(bc, buffer, HASH_LENGTH);
  
}

void bc_add_item_int(BitmapCounter bc, int item) {

    /* allocate buffer (for the hash result) */
    unsigned char buffer[HASH_LENGTH];
    
    /* get the hash */
    bc_hash_int(buffer, item);

    bc_add_hash(bc, buffer, HASH_LENGTH);
  
}

void bc_reset(BitmapCounter bc) {
    
    int i = 0;
    int bitmapSize = bc->nbits / sizeof(char);
    
    for (i = 0; i < bitmapSize; i++) {
        bc->bitmap[i] = 0;
    }
    
    bc->level = 0;
    
}