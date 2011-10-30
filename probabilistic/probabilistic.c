/* The paper defines the algorithm using "maps", i.e. a set of bitmaps with length
 * defined based on expected number of distinct values (4B means 2^32 values etc.)
 * which should be enough in most cases. And to update the N bitmaps they compute
 * N hashes using N salts, where the hash function produces results of the same
 * length as the bitmap (i.e. if the bitmaps are 4B long each, the hash returns
 * values between 1 and 2^32 etc.).
 * 
 * But we're using md5, and that always produces 16B result - it's be a waste of
 * CPU to use just the first 4B of output and throw away the rest of the hash. So
 * we're going to define the algorithm a bit differently - we'll use the whole
 * hash, but we'll split it into parts (of the defined length) and use each 
 * part as a separate hash. So for example by using 4B bitmaps, each MD5 hash
 * gives us 4 hashes.
 * 
 * So by using 4B bitmaps and 16 salts, we're effectively using 64 bitmaps
 * (because 16/4 * 16 = 64).
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "probabilistic.h"
#include "postgres.h"
#include "libpq/md5.h"

#define HASH_LENGTH 16

int pc_estimate(ProbabilisticCounter pc);

int pc_get_r(const unsigned char * buffer, int byteFrom, int bytes);
int pc_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes);

void pc_hash_text(unsigned char * buffer, char salt, const char * element, int elen);
void pc_hash_int(unsigned char * buffer, char salt, int element);

/* Allocate bitmap with a given length (to store the given number of elements).
 * 
 * nbytes - bytes per bitmap (to effectively use the hash, use powers of 2)
 *          4 is a good starting point in most cases (quite precise etc.)
 * nsalts - number of MD5 hashes to compute
 * 
 * To compute the actual number of bitmaps (the original paper states that
 * 64 bitmaps, each 4B long, mean about 10% error), do this:
 * 
 *    floor(16/nbytes) * nsalts
 * 
 * Generally using nbytes=4 and nsalts=32 is a good starting point.
 */
ProbabilisticCounter pc_create(int nbytes, int nsalts) {
  
    /* the bitmap is allocated as part of this memory block (-1 as one char is already in) */
    ProbabilisticCounter p = (ProbabilisticCounter)palloc(sizeof(ProbabilisticCounterData) + nsalts * HASH_LENGTH - 1);
    
    memset(p->bitmap, 0, nsalts * HASH_LENGTH);
    
    SET_VARSIZE(p, sizeof(ProbabilisticCounterData) + nsalts * HASH_LENGTH - VARHDRSZ);
    
    p->nbytes = nbytes;
    p->nsalts = nsalts;
    
    return p;
  
}

int pc_size(int nbytes, int nsalts) {
    return sizeof(ProbabilisticCounterData) + nsalts * HASH_LENGTH;
}

/* searches for the leftmost 1 */
int pc_get_min_bit(const unsigned char * buffer, int byteFrom, int bytes) {
  
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
int pc_get_r(const unsigned char * buffer, int byteFrom, int bytes) {
  
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

int pc_estimate(ProbabilisticCounter pc) {
  
    int salt = 0, slice = 0;
    float bits = 0;
    
    /* for each salt and each slice, get the estimate */
    for (salt = 0; salt < pc->nsalts; salt++) {
        for (slice = 0; slice < (HASH_LENGTH/pc->nbytes); slice++) {
        
            bits += (float)pc_get_r(
                                pc->bitmap,
                                (salt * HASH_LENGTH) + (slice * pc->nbytes), pc->nbytes
                           ) / (HASH_LENGTH/pc->nbytes * pc->nsalts);
        
        }
    }
    
    return powf(2, bits)/0.77351; /* magic constant, as listed in the paper */
  
}

void pc_hash_text(unsigned char * buffer, char salt, const char * element, int elen) {
    
    unsigned char item[elen + 1];
    
    memcpy(item, &salt, 1);
    memcpy(item+1, element, elen);
  
    pg_md5_binary(item, elen + 1, buffer);

}

void pc_hash_int(unsigned char * buffer, char salt, int element) {
  
    unsigned char item[5];
    
    memcpy(item, &salt, 1);
    memcpy(item+1, &element, 4);
  
    pg_md5_binary(item, 5, buffer);

}

void pc_add_element_text(ProbabilisticCounter pc, char * element, int elen) {
  
    /* get the hash */
    unsigned char hash[HASH_LENGTH];
    
    int salt, slice;
    
    /* compute hash for each salt, split the hash into pc->nbytes slices */
    for (salt = 0; salt < pc->nsalts; salt++) {
        
        /* compute the hash using the salt */
        pc_hash_text(hash, salt, element, elen);
        
        /* for each salt, process all the slices */
        for (slice = 0; slice < (HASH_LENGTH / pc->nbytes); slice++) {
        
            /* get the min bit (but skip the previous slices) */
            int bit = pc_get_min_bit(hash, (slice * pc->nbytes), pc->nbytes);
            
            /* get the current byte/bit index */
            int byteIdx = (HASH_LENGTH * salt) + (slice * pc->nbytes) + bit / 8;
            int bitIdx = bit % 8;
            
            /* set the bit of the bitmap */
            pc->bitmap[byteIdx] = pc->bitmap[byteIdx] | (0x1 << bitIdx);
        
        }
    }
}

void pc_add_element_int(ProbabilisticCounter pc, int element) {
  
    /* get the hash */
    unsigned char hash[HASH_LENGTH];
    
    int salt, slice;
    
    /* compute hash for each salt, split the hash into pc->nbytes slices */
    for (salt = 0; salt < pc->nsalts; salt++) {
        
        /* compute the hash using the salt */
        pc_hash_int(hash, salt, element);
        
        /* for each salt, process all the slices */
        for (slice = 0; slice < (HASH_LENGTH / pc->nbytes); slice++) {
        
            /* get the min bit (but skip the previous slices) */
            int bit = pc_get_min_bit(hash, (slice * pc->nbytes), pc->nbytes);
            
            /* get the current byte/bit index */
            int byteIdx = (HASH_LENGTH * salt) + (slice * pc->nbytes) + bit / 8;
            int bitIdx = bit % 8;
            
            /* set the bit of the bitmap */
            pc->bitmap[byteIdx] = pc->bitmap[byteIdx] | (0x1 << bitIdx);
        
        }
    }
}

void pc_reset(ProbabilisticCounter pc) {
    int i;
    for (i = 0; i < pc->nsalts * HASH_LENGTH; i++) {
        pc->bitmap[i] = 0;
    }
}