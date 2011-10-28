#include "postgres.h"

/* Structure representing the current state of a self-learning bitmap,
 * as described in the paper "Distinct Counting with a Self-Learning
 * Bitmap" (by Aiyou Chen and Jin Cao, published in 2009).
 */
typedef struct BitmapCounterData {
    
    /* length of the bitmap (total bytes) */
    int32 length;
  
    /* number of bits of the key 'c' (bitmap index) and 'd' */
    int cbits;
    int dbits;
    
    /* size of the bitmap (2^c) */
    int nbits;
    
    /* level */
    int level;
    
    /* requested error ratio */
    float error;
    
    /* number of distinct values */
    int ndistinct;
    
    /* helper (used to compute a lot of other values) */
    float r;
    
    /* bitmap used to keep the list of items (uses the very same trick as in
     * the varlena type in include/c.h */
    unsigned char bitmap[1];
    
} BitmapCounterData;

/* Just a pointer to the data, to that it's easier to work with it. */
typedef BitmapCounterData* BitmapCounter;

/* Creates a self-learning bitmap that is able to count up to the number
 * of distinct values with the given error rate. */
BitmapCounter bc_init(float error, int ndistinct);

/* Add an element to the bitmap. This implements the UPDATE described
 * on page 2 of the paper.
 *
 * bc     - the bitmap counter
 * item   - an item to add to the bitmap
 * length - length of the item (in bytes)
 */
void bc_add_element(BitmapCounter bc, const char * item, int length);

/* Computes estimate of ndistinct using the current state of the counter */
int bc_estimate(BitmapCounter bc);

/* Reset the counter (so that it seems to e empty) */
void bc_reset(BitmapCounter bc);

/* add element into the counter */
void bc_add_item_text(BitmapCounter bc, const char * item, int length);
void bc_add_item_int(BitmapCounter bc, int item);

/* print info about the counter */
void bc_print_info(BitmapCounter ac);

/* get estimate */
int bc_estimate(BitmapCounter ac);
