#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "adaptive.h"
#include "postgres.h"

#define HASH_LENGTH 16

#include "libpq/md5.h"

/* internal hash operations */
void ac_hash_text(unsigned char * buffer, const char * element, int elen);
void ac_hash_int(unsigned char * buffer, int element);
int  ac_hash_matches(const unsigned char * hash, int level);
void ac_split(AdaptiveCounter ac);
int  ac_in_list(AdaptiveCounter ac, unsigned char * hash);
void ac_add_hash(AdaptiveCounter ac, unsigned char * hash);

/* allocate adaptive counter with a given error rate */
AdaptiveCounter ac_init(float error, int ndistinct) {
  
    int itemSize;
    AdaptiveCounter p;
    size_t length;

    int sizeA, sizeB;
    
    /* This rule comes right from the adaptive sampling paper */
    int maxItems = ceil(powf(1.2/error, 2));
    
    /* We need to use such item size that suffices two requirements
     * 
     * (a) Given the expected ndistinct value, there needs to be enough
     *     bits to hold the value (2^level >= ndistinct)
     * 
     * (b) The hash needs to be large enough that the hash collisions,
     *     when adding it to the list, are not too frequent.
     * 
     * So we'll compute sizes for both requirements and use the higher one.
     * 
     * For (a) we could get simply log_2(ndistinct) bits, but this depends
     * on the number of items too (see ac_estimate), we can divide it by
     * the capacity of the list).
     * 
     * For (b) we can simply get enough bits to cover the expected number
     * of distinct values - that's log_256(ndistinct) bytes.
     * 
     * We must not forget that as we increase the level, the number of
     * bits that may differ is actually decreasing. That's why we add
     * one more byte in both cases - that should give at least 1:256
     * probability of collisions in the ideal case  (the actual number
     * of remaining bits will be a bit higher actually, as the 2^level
     * is usually not rounded to bytes).
     * 
     * Be aware that the maxItems may actually be lower than 256, depending 
     * on the demanded precision - for error rates higher than 7.5% the
     * maxItems is actually lower than 256.
     */
    
    /* in bytes */
    sizeA = ceilf((logf(ndistinct / maxItems) / logf(2)) / 8) + 1;
    sizeB = ceilf(logf(ndistinct) / logf(256)) + 1;
    
    itemSize = (sizeA > sizeB) ? sizeA : sizeB;
    
    /* the bitmap is allocated as part of this memory block (-1 as one char is already in) */
    length = offsetof(AdaptiveCounterData,bitmap) + (itemSize * maxItems);
    p = (AdaptiveCounter)palloc(length);
  
    /* No need to zero the memory, we'll keep track of empty/used space using the variables below. */
  
    /* store the length too (including the length field) */
    SET_VARSIZE(p, length);
  
    p->maxItems = maxItems;
    p->itemSize = itemSize;
    p->error = error;
    p->ndistinct = ndistinct;
    
    p->level = 0;
    p->items = 0;
    
    return p;

}

/* resets the counter (so it behaves as if it's empty) */
void ac_reset(AdaptiveCounter ac) {
    ac->items = 0;
    ac->level = 0;
}

/* creates a separate copy of the counter */
AdaptiveCounter ac_create_copy(AdaptiveCounter src) {

    AdaptiveCounter dest = (AdaptiveCounter)palloc(VARSIZE(src));
    
    memcpy(dest, src, VARSIZE(src));

    return dest;
    
}

/* get the current estimate */
int ac_estimate(AdaptiveCounter ac) {
    
    int estimate = powf(2, ac->level) * ac->items;
    
    return estimate;
  
}

/* compute hash of the text element (and stores it in the buffer) */
void ac_hash_text(unsigned char * buffer, const char * element, int elen) {

    pg_md5_binary(element, elen, buffer);

}

/* computes a hash of the int element (and stores it in the buffer) */
void ac_hash_int(unsigned char * buffer, int element) {

    pg_md5_binary(&element, sizeof(int), buffer);

}

/* Check if the hash matches the current level (number of 1s at the beginning).
   Returns 1 if it matches the level, 0 otherwise. */
int ac_hash_matches(const unsigned char * hash, int level) {
  
    int bit = 0;
    
    /* check if first 'level' bits are 1 */
    for (bit = 0; bit < level; bit++) {
        
        int byteIdx = bit / 8;
        int bitIdx = bit % 8;
        
        /* the bit is 0 - return FALSE */
        if (! (hash[byteIdx] & (1 << bitIdx))) {
            return 0;
        }
    }
    
    /* first level bits are all 1 - return TRUE */
    return 1;
  
}

/* perform 'split' - increase the level and remove non-matching items from the list */
void ac_split(AdaptiveCounter ac) {

    /* find first not-matching item, then find last matching item and swap them */
    int itemIdx = 0;
  
    /* split should happen only when the list is full */
    if (ac->items != ac->maxItems) {
        elog(ERROR, "The counter is not full, can't split (items = %d, max = %d)",
            ac->items, ac->maxItems
        );
    }
    
    /* check if we can split (when level reaches itemSize*8, we're can't split further) */
    if (ac->itemSize*8 == ac->level) {
        elog(ERROR, "The counter capacity is exhausted, can't split further (level = %d, item size = %d bits)",
            ac->level, ac->itemSize*8
        );
    }
  
    /* increment the level */
    ac->level = ac->level + 1;
  
    /* remove the items that do not match the new level */
    for (itemIdx = 0; itemIdx < ac->items; itemIdx++) {
    
        /* if the hash does not matches the current level, find a replacement */
        if (! ac_hash_matches(&(ac->bitmap[itemIdx*ac->itemSize]), ac->level)) {
        
            /* look for the last matching item (reverse) */
            while (! ac_hash_matches(&(ac->bitmap[(ac->items-1)*ac->itemSize]), ac->level)) {
                ac->items = ac->items - 1;
            }
            
            /* there is a matching item, so let's swap it */
            if ((ac->items - 1) > itemIdx) {
                memcpy(&(ac->bitmap[itemIdx*ac->itemSize]), &(ac->bitmap[(ac->items-1)*ac->itemSize]), ac->itemSize);
                ac->items = ac->items - 1;
            }
            
        }
    }
  
    /* There's very small probability that the split does not remove any item (all items
     * already match the new level) -> run the split again */
    if (ac->items == ac->maxItems) {
        ac_split(ac);
    }
  
}

/* check if the item (it's hash) is already in the list - returns 1 if it's on the list,
   0 otherwise */
int ac_in_list(AdaptiveCounter ac, unsigned char * hash) {
  
    int idx;
    for (idx = 0; idx < ac->items; idx++) {
        if (memcmp(&(ac->bitmap[idx*ac->itemSize]), hash, ac->itemSize) == 0) {
            return 1;
        }
    }
  
    return 0;
    
}

/* FIXME The split is executed after the insertion, not before it (as described in the
 * paper). Not sure if this may change the precision or something like that. */
void ac_add_hash(AdaptiveCounter ac, unsigned char * hash) {
  
    /* check if the hash matches the level or the item is already in the list */
    if ((! ac_hash_matches(hash, ac->level)) || (ac_in_list(ac, hash))) {
        return;
    }
  
    /* add the hash into the list */
    memcpy(&(ac->bitmap[ac->items * ac->itemSize]), hash, ac->itemSize);
    ac->items += 1;
  
    /* check if the list is full - if yes, split (increment the level and remove items
     * that do not match the current state) */
    if (ac->items == ac->maxItems) {
        ac_split(ac);
    }
  
}

/* Adds an element into the counter - computes a hash and the call ac_add_hash.
 *
 * If you already know the hash, use ac_add_hash directly.
 */
void ac_add_item(AdaptiveCounter ac, const char * element, int elen) {
  
    /* get the hash */
    unsigned char hash[HASH_LENGTH];
  
    /* compute the hash */
    ac_hash_text(hash, element, elen);
  
    ac_add_hash(ac, hash);
  
}

/* Merge an adaptive counter into another one. The first parameter 'dest' is the target
 * counter that will be modified during the merge.
 *
 * The counters have to be 'the same' i.e. the basic parameters (level, length, itemSize
 * and error rate) need to be equal.
 */
AdaptiveCounter ac_merge(AdaptiveCounter dest, AdaptiveCounter src) {

    AdaptiveCounter result;
    int i = 0;
  
    /* check if we need to swap dest/src - we need the destination to have
    * higher (>=) level and lower item size (<=) at the same time */
    if ((dest->level < src->level) ||
        ((dest->level == src->level) && (dest->itemSize > src->itemSize))) {
        return ac_merge(src, dest);
    }
  
    /* it's possible to have (dest->level > src->level) and
    * (dest->itemSize > src->itemSize) at the same time, which makes
    * the counters impossible to merge. */
  
    /* check that the counters are 'mergeable' - there are two
    * conditions: 
    * 
    * 1) Lower level has to be merged into the higher level (this is
    *    assured thanks to the previous swap, so we have assert here).
    * 
    * 2) The destination item size must not be higher than the source.
    * 
    * Anyway the best way to make sure two counters are mergeable is
    * to use exactly the same counters (error rate, item length).
    * 
    * Maybe there should be another condition on maxItems, but I don't
    * think so.
    */
    assert(dest->level >= src->level);
  
    if (dest->itemSize > src->itemSize) {
        elog(ERROR, "counters not mergeable - item length dest=%d > src=%d ",
            dest->itemSize, src->itemSize);
    }

    /* allocate space for the destination counter (mergeable -> same size) */
    result = ac_create_copy(dest);
  
    /* copy the items (from the src counter, the one with the lower level) */
    for (i = 0; i < src->items; i++) {
        ac_add_hash(result, &(src->bitmap[i*src->itemSize]));
    }
  
    return result;
  
}
