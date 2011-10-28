/* Length of hash (MD5 => 16B) */
#define HASH_LENGTH 16

/* This is an implementation of "probabilistic counter" as described in the
 * article "Probalistic Counting Algorithms for Data Base Applications",
 * published by Flajolet and Martin in 1985. */
typedef struct ProbabilisticCounterData {
    
    /* length of the struncture (in this case equal to sizeof) */
    int length;
    
    /* number of bytes per bitmap */
    int nbytes;
    
    /* number of salts */
    int nsalts;
    
    /* bitmap used to keep the list of items (uses the very same trick as in
     * the varlena type in include/c.h */
    unsigned char bitmap[1];
    
} ProbabilisticCounterData;

typedef ProbabilisticCounterData* ProbabilisticCounter;

/* creates an optimal bloom filter for the given bitmap size and number of distinct values */
ProbabilisticCounter pc_create(int nbytes, int nsalts);
int pc_size(int nbytes, int nsalts);

/* add element existence */
void pc_add_element_text(ProbabilisticCounter pc, char * element, int elen);
void pc_add_element_int(ProbabilisticCounter pc, int element);
    
/* print info about the counter */
void pc_print_info(ProbabilisticCounter pc);

/* get current estimate */
int pc_estimate(ProbabilisticCounter pc);

void pc_reset(ProbabilisticCounter pc);
