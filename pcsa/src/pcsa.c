#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "postgres.h"
#include "libpq/md5.h"

#include "pcsa.h"

#define HASH_LENGTH 16

int pcsa_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes);
int pcsa_get_r(const unsigned char * buffer, int byteFrom, int bytes);
int pcsa_estimate(PCSACounter pcsa);

void pcsa_add_hash(PCSACounter pcsa, const unsigned char * hash);
void pcsa_reset_internal(PCSACounter pcsa);

/* allocate bitmap with a given length (to store the given number of bitmaps) */
PCSACounter pcsa_create(int nmaps, int keysize) {
  
  /* the bitmap is allocated as part of this memory block (-1 as one char is already in) */
  PCSACounter p;
  int i = 0;
  
  size_t length = offsetof(PCSACounterData,bitmap) + (HASH_LENGTH - keysize) * nmaps;
  
  p = (PCSACounter)palloc(length);
  
  for (i = 0; i < (HASH_LENGTH - keysize) * nmaps; i++) {
      p->bitmap[i] = 0;
  }

  SET_VARSIZE(p, length);
  
  p->nmaps = nmaps;
  p->keysize = keysize;
  
  return p;
  
}

int pcsa_get_size(int nmaps, int keysize) {
    return offsetof(PCSACounterData,bitmap) + (HASH_LENGTH - keysize) * nmaps;
}

/* searches for the leftmost 1 */
int pcsa_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes) {
  
    int k = 0;
    int byteIdx = 0;
    int bitIdx = 0;
    
    for (k = byteFrom * 8; k < ((byteFrom + bytes) * 8); k++) {
        
        byteIdx = k / 8;
        bitIdx=  k % 8;
        
        if ((buffer[byteIdx] & (0x1 << bitIdx)) != 0) {
            return k - byteFrom *8;
        }
        
    }
    
    return (HASH_LENGTH*8);
  
}

/* searches for the leftmost zero */
int pcsa_get_r(const unsigned char * buffer, int byteFrom, int bytes) {
  
    int k = 0;
    int byteIdx = 0;
    int bitIdx = 0;
    
    for (k = byteFrom * 8; k < ((byteFrom + bytes) * 8); k++) {
        
        byteIdx = k / 8;
        bitIdx=  k % 8;
        
        if ((buffer[byteIdx] & (0x1 << bitIdx)) == 0) {
            return k - byteFrom*8;
        }
        
    }
    
    return (HASH_LENGTH*8);
  
}

int pcsa_estimate(PCSACounter pcsa) {
  
    float bits = 0;
    int bitmap;
    
    /* get the estimate for each bitmap */
    for (bitmap = 0; bitmap < pcsa->nmaps; bitmap++) {
        bits += pcsa_get_r(pcsa->bitmap, (bitmap * (HASH_LENGTH - pcsa->keysize)), (HASH_LENGTH - pcsa->keysize));
    }
    
    return (pcsa->nmaps / 0.77351) * powf(2, bits/pcsa->nmaps);
  
}

void pcsa_add_element(PCSACounter pcsa, const char * element, int elen) {
  
    /* get the hash */
    unsigned char hash[HASH_LENGTH];
    
    /* compute the hash */
    pg_md5_binary(element, elen, hash);
    
    /* add the hash to the counter */
    pcsa_add_hash(pcsa, hash);
  
}

void pcsa_add_hash(PCSACounter pcsa, const unsigned char * hash) {
  
    /* get the hash */
    unsigned int bitmapIdx;
    int bit;
    int byteIdx;
    int bitIdx;
    
    memcpy(&bitmapIdx, hash, pcsa->keysize);
    
    bitmapIdx = bitmapIdx % pcsa->nmaps;
    
    /* get the min bit (but skip the bytes used for key) */
    bit = pcsa_get_min_bit(hash, pcsa->keysize, HASH_LENGTH - pcsa->keysize);
        
    /* get the current byte/bit index */
    byteIdx = (HASH_LENGTH - pcsa->keysize) * bitmapIdx + bit / 8;
    bitIdx = bit % 8;
        
    /* set the bit of the bitmap */
    pcsa->bitmap[byteIdx] = pcsa->bitmap[byteIdx] | (0x1 << bitIdx);
  
}

void pcsa_reset_internal(PCSACounter pcsa) {
    
    int i;
    for (i = 0; i < (HASH_LENGTH - pcsa->keysize) * pcsa->nmaps; i++) {
        pcsa->bitmap[i] = 0;
    }
  
}