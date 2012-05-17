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
LogLogCounter loglog_create(float error, int ndistinct) {

  int size = loglog_get_size(error, ndistinct);

  /* the bitmap is allocated as part of this memory block (-1 as one char is already in) */
  LogLogCounter p = (LogLogCounter)palloc(size);
  
  p->nmaps = 1.3 / (error * error);
  p->k = (int)ceil(log2(p->nmaps));
  p->nmaps = (int)pow(2, p->k);
  
  p->keysize = (p->k+7)/8; // FIXME not more than 4B (see loglog_add_hash)
  p->mapsize = 1; // FIXME compute properly (int)ceil(log2(log2(ndistinct))/8);
  
  memset(p->bitmap, 0, p->mapsize * p->nmaps);
  
  SET_VARSIZE(p, size - VARHDRSZ);
  
  return p;
  
}

int loglog_get_size(float error, int ndistinct) {

  int mapsize = 1;
  int nmaps = 1.3 / (error * error);
  int k = (int)ceil(log2(nmaps));
  
  nmaps = (int)pow(2, k);

  // FIXME int mapsize = (int)ceil(log2(log2(ndistinct))/8);

  return sizeof(LogLogCounterData) + nmaps * mapsize;

}

/* searches for the leftmost 1 */
int loglog_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes) {
  
    int k = 0;
    int byteIdx = 0;
    int bitIdx = 0;
    
    for (k = byteFrom * 8; k < ((byteFrom + bytes) * 8); k++) {
        
        byteIdx = k / 8;
        bitIdx=  k % 8;
        
        if ((buffer[byteIdx] & (0x1 << bitIdx)) != 0) {
            return k - byteFrom *8 + 1;
        }
        
    }
    
    return (bytes*8);
  
}

int loglog_estimate(LogLogCounter loglog) {
  
    int sum = 0;
    int bitmap;
    
    /* get the estimate for each bitmap */
    for (bitmap = 0; bitmap < loglog->nmaps; bitmap++) {
        sum += (unsigned char) loglog->bitmap[bitmap];
    }
    
    return 0.783 * loglog->nmaps * powf(2, sum / loglog->nmaps);

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
    unsigned int bitmapIdx;
    int idx;
    int rho;
    
    memcpy(&bitmapIdx, hash, loglog->keysize);
    
    idx  = bitmapIdx >> (loglog->keysize*8 - loglog->k);
    
    /* get the min bit (but skip the bytes used for key) */
    rho = loglog_get_min_bit(hash, loglog->keysize, HASH_LENGTH - loglog->keysize);
        
    /* set the bit of the bitmap */
    if (rho > (unsigned char)(loglog->bitmap[idx])) {
        loglog->bitmap[idx] = (unsigned char)rho;
    }

}

void loglog_reset_internal(LogLogCounter loglog) {
    
    memset(loglog->bitmap, 0, loglog->nmaps * loglog->mapsize);

}