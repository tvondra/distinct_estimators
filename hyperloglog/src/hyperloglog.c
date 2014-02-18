#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "postgres.h"
#include "libpq/md5.h"

#include "hyperloglog.h"

/* we're using md5, which produceds a 16B value */
#define HASH_LENGTH 16

/* constants for alpha */
static float alpha[] = {0, 0, 0, 0, 0.673, 0.697, 0.709, 0.7153, 0.7183, 0.7198, 0.7205, 0.7209, 0.7211, 0.7212, 0.7213, 0.7213, 0.7213};

int hyperloglog_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes);
int hyperloglog_get_r(const unsigned char * buffer, int byteFrom, int bytes);
int hyperloglog_estimate(HyperLogLogCounter hloglog);

void hyperloglog_hash_text(unsigned char * buffer, const char * element, int length);
void hyperloglog_hash_int(unsigned char * buffer, int element);

void hyperloglog_add_hash(HyperLogLogCounter hloglog, const unsigned char * hash);
void hyperloglog_reset_internal(HyperLogLogCounter hloglog);

/* allocate bitmap with a given length (to store the given number of bitmaps) */
HyperLogLogCounter hyperloglog_create(float error) {

  float m;
  int size = hyperloglog_get_size(error);

  /* the bitmap is allocated as part of this memory block (-1 as one char is already in) */
  HyperLogLogCounter p = (HyperLogLogCounter)palloc(size);
  
  if (error <= 0 || error >= 1)
      elog(ERROR, "invalid error rate requested");
  
  m = 1.04 / (error * error);
  
  p->b = (int)ceil(log2(m));
  
  /* TODO Is there actually a good reason to limit the number precision to 16 bits? We're
   * using MD5, so we have 128 bits available ... It'll require more memory but if thats
   * what was requested, why not to allow that? */
  
  if (p->b < 4)
      p->b = 4;
  else if (p->b > 16)
      elog(ERROR, "number of bits in HyperLogLog exceeds 16");
           
  p->m = (int)pow(2, p->b);
  memset(p->data, 0, p->m);
  
  SET_VARSIZE(p, size - VARHDRSZ);
  
  return p;
  
}

int hyperloglog_get_size(float error) {

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

  return sizeof(HyperLogLogCounterData) + (int)pow(2, b);

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

int hyperloglog_estimate(HyperLogLogCounter hloglog) {
  
    double sum = 0, E = 0;
    int j;
    
    /* compute the sum for the indicator function */
    for (j = 0; j < hloglog->m; j++) {
        sum += (1.0 / pow(2, hloglog->data[j]));
    }
    
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

/* Computes an MD5 hash of the input value (with a given length). */
void hyperloglog_hash_text(unsigned char * buffer, const char * element, int length) {
    pg_md5_binary(element, length, buffer);
}

/* Computes an MD5 hash of the input value (with a given length). */
void hyperloglog_hash_int(unsigned char * buffer, int element) {
    pg_md5_binary(&element, sizeof(int), buffer);
}

void hyperloglog_add_element_text(HyperLogLogCounter hloglog, const char * element, int elen) {
  
    /* get the hash */
    unsigned char hash[HASH_LENGTH];
    
    /* compute the hash using the salt */
    hyperloglog_hash_text(hash, element, elen);
    
    hyperloglog_add_hash(hloglog, hash);
  
}

void hyperloglog_add_element_int(HyperLogLogCounter hloglog, int element) {

    /* allocate buffer (for the hash result) */
    unsigned char buffer[HASH_LENGTH];
    
    /* get the hash */
    hyperloglog_hash_int(buffer, element);

    hyperloglog_add_hash(hloglog, buffer);
  
}

void hyperloglog_add_hash(HyperLogLogCounter hloglog, const unsigned char * hash) {
  
    /* get the hash */
    unsigned int idx;
    char rho;

    /* which stream is this (keep only the first 'b' bits) */
    memcpy(&idx, hash, sizeof(int));
    idx  = idx >> (32 - hloglog->b);
    
    /* get the min bit (but skip the bits used for stream index) */
    
    /* TODO Originally this was
     * 
     *      rho = hyperloglog_get_min_bit(&hash[4], hloglog->b, 64);
     * 
     * but that made the estimates much much worse for some reason. Hmm,
     * maybe the shifts work differently than I thought and so it was
     * somehow coupled to the 'idx'? */
    
    rho = hyperloglog_get_min_bit(&hash[4], 0, 64); /* 64-bit hash */
    
    /* keep the highest value */
    hloglog->data[idx] = (rho > (hloglog->data[idx])) ? rho : hloglog->data[idx];

}

void hyperloglog_reset_internal(HyperLogLogCounter hloglog) {
    
    memset(hloglog->data, 0, hloglog->m);

}