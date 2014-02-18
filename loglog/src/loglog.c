#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "postgres.h"
#include "libpq/md5.h"

#include "loglog.h"

#define HASH_LENGTH 16

int loglog_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes);
int loglog_get_r(const unsigned char * buffer, int byteFrom, int bytes);
int loglog_estimate(LogLogCounter loglog);

void loglog_hash_text(unsigned char * buffer, const char * element, int length);
void loglog_hash_int(unsigned char * buffer, int element);

void loglog_add_hash(LogLogCounter loglog, const unsigned char * hash);
void loglog_reset_internal(LogLogCounter loglog);

/* allocate bitmap with a given length (to store the given number of bitmaps) */
LogLogCounter loglog_create(float error) {

  float m;
  int size = loglog_get_size(error);

  /* the bitmap is allocated as part of this memory block (-1 as one char is already in) */
  LogLogCounter p = (LogLogCounter)palloc(size);
  
  m = 1.3 / (error * error);
  p->bits  = (int)ceil(log2(m));
  p->m = (int)pow(2, p->bits);
  
  memset(p->data, -1, p->m);
  
  SET_VARSIZE(p, size - VARHDRSZ);
  
  return p;
  
}

int loglog_get_size(float error) {

  float m = 1.3 / (error * error);
  int bits = (int)ceil(log2(m));

  return sizeof(LogLogCounterData) + (int)pow(2, bits);

}

/* searches for the leftmost 1 (aka 'rho' in the algorithm) */
int loglog_get_min_bit(const unsigned char * buffer, int bitfrom, int nbits) {
  
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

int loglog_estimate(LogLogCounter loglog) {
  
    int j;
    float sum = 0;
    
    /* get the estimate for each bitmap */
    for (j = 0; j < loglog->m; j++) {
        sum += loglog->data[j];
    }
    
    return 0.39701 * loglog->m * powf(2, sum / loglog->m);

}

/* Computes an MD5 hash of the input value (with a given length). */
void loglog_hash_text(unsigned char * buffer, const char * element, int length) {
    pg_md5_binary(element, length, buffer);
}

/* Computes an MD5 hash of the input value (with a given length). */
void loglog_hash_int(unsigned char * buffer, int element) {
    pg_md5_binary(&element, sizeof(int), buffer);
}

void loglog_add_element_text(LogLogCounter loglog, const char * element, int elen) {
  
    /* get the hash */
    unsigned char hash[HASH_LENGTH];
    
    /* compute the hash using the salt */
    loglog_hash_text(hash, element, elen);
    
    loglog_add_hash(loglog, hash);
  
}

void loglog_add_element_int(LogLogCounter loglog, int element) {

    /* allocate buffer (for the hash result) */
    unsigned char buffer[HASH_LENGTH];
    
    /* get the hash */
    loglog_hash_int(buffer, element);

    loglog_add_hash(loglog, buffer);
  
}

void loglog_add_hash(LogLogCounter loglog, const unsigned char * hash) {
  
    /* get the hash */
    unsigned int idx;
    char rho;

    /* which stream is this (keep only the first 'b' bits) */
    memcpy(&idx, hash, sizeof(int));
    idx  = idx >> (32 - loglog->bits);
    
    /* get the min bit (but skip the bits used for stream index) */
    
    /* TODO Originally this was
     * 
     *      rho = hyperloglog_get_min_bit(&hash[4], hloglog->b, 64);
     * 
     * but that made the estimates much much worse for some reason. Hmm,
     * maybe the shifts work differently than I thought and so it was
     * somehow coupled to the 'idx'? */
    
    rho = loglog_get_min_bit(&hash[4], 0, 64); /* 64-bit hash */
    
    /* keep the highest value */
    loglog->data[idx] = (rho > (loglog->data[idx])) ? rho : loglog->data[idx];

}

void loglog_reset_internal(LogLogCounter loglog) {
    
    memset(loglog->data, 0, loglog->m);

}