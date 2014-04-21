#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "postgres.h"
#include "fmgr.h"
#include "pcsa.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define VAL(CH)         ((CH) - '0')
#define DIG(VAL)        ((VAL) + '0')

#define DEFAULT_NBITMAPS    64
#define DEFAULT_KEYSIZE     4

#define MAX_KEYSIZE         4
#define MAX_BITMAPS         2048

PG_FUNCTION_INFO_V1(pcsa_add_item);
PG_FUNCTION_INFO_V1(pcsa_add_item_agg);
PG_FUNCTION_INFO_V1(pcsa_add_item_agg2);

PG_FUNCTION_INFO_V1(pcsa_merge_agg);
PG_FUNCTION_INFO_V1(pcsa_merge_simple);
PG_FUNCTION_INFO_V1(pcsa_get_estimate);
PG_FUNCTION_INFO_V1(pcsa_size);
PG_FUNCTION_INFO_V1(pcsa_init);
PG_FUNCTION_INFO_V1(pcsa_reset);
PG_FUNCTION_INFO_V1(pcsa_in);
PG_FUNCTION_INFO_V1(pcsa_out);
PG_FUNCTION_INFO_V1(pcsa_rect);
PG_FUNCTION_INFO_V1(pcsa_send);
PG_FUNCTION_INFO_V1(pcsa_length);

Datum pcsa_add_item(PG_FUNCTION_ARGS);
Datum pcsa_add_item_agg(PG_FUNCTION_ARGS);
Datum pcsa_add_item_agg2(PG_FUNCTION_ARGS);

Datum pcsa_merge_agg(PG_FUNCTION_ARGS);
Datum pcsa_merge_simple(PG_FUNCTION_ARGS);
Datum pcsa_get_estimate(PG_FUNCTION_ARGS);
Datum pcsa_size(PG_FUNCTION_ARGS);
Datum pcsa_init(PG_FUNCTION_ARGS);
Datum pcsa_reset(PG_FUNCTION_ARGS);
Datum pcsa_in(PG_FUNCTION_ARGS);
Datum pcsa_out(PG_FUNCTION_ARGS);
Datum pcsa_recv(PG_FUNCTION_ARGS);
Datum pcsa_send(PG_FUNCTION_ARGS);
Datum pcsa_length(PG_FUNCTION_ARGS);


Datum
pcsa_add_item(PG_FUNCTION_ARGS)
{

    PCSACounter pcsa;

    /* requires the estimator to be already created */
    if (PG_ARGISNULL(0))
        elog(ERROR, "pcsa counter must not be NULL");

    /* if the element is not NULL, add it to the estimator (i.e. skip NULLs) */
    if (! PG_ARGISNULL(1)) {

        Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
        Datum       element = PG_GETARG_DATUM(1);
        int16       typlen;
        bool        typbyval;
        char        typalign;

        /* estimator (we know it's not a NULL value) */
        pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);

        /* TODO The requests for type info shouldn't be a problem (thanks to lsyscache),
         * but if it turns out to have a noticeable impact it's possible to cache that
         * between the calls (in the estimator). */

        /* get type information for the second parameter (anyelement item) */
        get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

        /* it this a varlena type, passed by reference or by value ? */
        if (typlen == -1) {
            /* varlena */
            pcsa_add_element(pcsa, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            pcsa_add_element(pcsa, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            pcsa_add_element(pcsa, (char*)element, typlen);
        }

    }

    PG_RETURN_VOID();

}

Datum
pcsa_add_item_agg(PG_FUNCTION_ARGS)
{

    PCSACounter pcsa;
    int  bitmaps; /* number of bitmaps */
    int  keysize; /* keysize */

    /* info for anyelement */
    Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
    Datum       element = PG_GETARG_DATUM(1);
    int16       typlen;
    bool        typbyval;
    char        typalign;

    /* create a new estimator (with requested error rate) or reuse the existing one */
    if (PG_ARGISNULL(0)) {

        bitmaps = PG_GETARG_INT32(2);
        keysize = PG_GETARG_INT32(3);
      
        /* key size has to be between 1 and 4, bitmaps between 1 and 2048 */
        if ((keysize < 1) || (keysize > MAX_KEYSIZE)) {
            elog(ERROR, "key size has to be between 1 and %d", MAX_KEYSIZE);
        } else if ((bitmaps < 1) || (bitmaps > MAX_BITMAPS)) {
            elog(ERROR, "number of bitmaps has to be between 1 and %d", MAX_BITMAPS);
        }
        
        pcsa = pcsa_create(bitmaps, keysize);

    } else { /* existing estimator */
        pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
    }

    /* add the item to the estimator (skip NULLs) */
    if (! PG_ARGISNULL(1)) {

        /* TODO The requests for type info shouldn't be a problem (thanks to lsyscache),
         * but if it turns out to have a noticeable impact it's possible to cache that
         * between the calls (in the estimator). */
        
        /* get type information for the second parameter (anyelement item) */
        get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

        /* it this a varlena type, passed by reference or by value ? */
        if (typlen == -1) {
            /* varlena */
            pcsa_add_element(pcsa, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            pcsa_add_element(pcsa, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            pcsa_add_element(pcsa, (char*)element, typlen);
        }
    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(pcsa);
    
}

Datum
pcsa_add_item_agg2(PG_FUNCTION_ARGS)
{

    PCSACounter pcsa;

    /* info for anyelement */
    Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
    Datum       element = PG_GETARG_DATUM(1);
    int16       typlen;
    bool        typbyval;
    char        typalign;

    /* create a new estimator (with requested error rate) or reuse the existing one */
    if (PG_ARGISNULL(0)) {
        pcsa = pcsa_create(DEFAULT_NBITMAPS, DEFAULT_KEYSIZE);
    } else { /* existing estimator */
        pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
    }

    /* add the item to the estimator (skip NULLs) */
    if (! PG_ARGISNULL(1)) {

        /* TODO The requests for type info shouldn't be a problem (thanks to lsyscache),
         * but if it turns out to have a noticeable impact it's possible to cache that
         * between the calls (in the estimator). */
        
        /* get type information for the second parameter (anyelement item) */
        get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

        /* it this a varlena type, passed by reference or by value ? */
        if (typlen == -1) {
            /* varlena */
            pcsa_add_element(pcsa, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            pcsa_add_element(pcsa, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            pcsa_add_element(pcsa, (char*)element, typlen);
        }
    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(pcsa);
}

Datum
pcsa_merge_simple(PG_FUNCTION_ARGS)
{

    PCSACounter counter1 = (PCSACounter)PG_GETARG_BYTEA_P(0);
    PCSACounter counter2 = (PCSACounter)PG_GETARG_BYTEA_P(1);

    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0) && PG_ARGISNULL(1)) {
        PG_RETURN_NULL();
    } else if (PG_ARGISNULL(0)) {
        PG_RETURN_BYTEA_P(pcsa_copy(counter2));
    } else if (PG_ARGISNULL(1)) {
        PG_RETURN_BYTEA_P(pcsa_copy(counter1));
    } else {
        PG_RETURN_BYTEA_P(pcsa_merge(counter1, counter2, false));
    }

}

Datum
pcsa_merge_agg(PG_FUNCTION_ARGS)
{

    PCSACounter counter1;
    PCSACounter counter2 = (PCSACounter)PG_GETARG_BYTEA_P(1);

    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0)) {

        /* just copy the second estimator into the first one */
        counter1 = pcsa_copy(counter2);

    } else {

        /* ok, we already have the estimator - merge the second one into it */
        counter1 = (PCSACounter)PG_GETARG_BYTEA_P(0);

        /* perform the merge (in place) */
        counter1 = pcsa_merge(counter1, counter2, true);

    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(counter1);

}

Datum
pcsa_get_estimate(PG_FUNCTION_ARGS)
{
  
    int estimate;
    PCSACounter pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
    
    /* in-place update works only if executed as aggregate */
    estimate = pcsa_estimate(pcsa);
    
    /* return the updated bytea */
    PG_RETURN_FLOAT4(estimate);

}

Datum
pcsa_init(PG_FUNCTION_ARGS)
{
      PCSACounter pcsa;
      int bitmaps;
      int keysize;
      
      bitmaps = PG_GETARG_INT32(0);
      keysize = PG_GETARG_INT32(1);
            
      /* key size has to be between 1 and 4, bitmaps between 1 and 2048 */
      if ((keysize < 1) || (keysize > MAX_KEYSIZE)) {
          elog(ERROR, "key size has to be between 1 and %d", MAX_KEYSIZE);
      } else if ((bitmaps < 1) || (bitmaps > MAX_BITMAPS)) {
          elog(ERROR, "number of bitmaps has to be between 1 and %d", MAX_BITMAPS);
      }
      
      pcsa = pcsa_create(bitmaps, keysize);
      
      PG_RETURN_BYTEA_P(pcsa);
}

Datum
pcsa_size(PG_FUNCTION_ARGS)
{
      int bitmaps;
      int keysize;
      
      bitmaps = PG_GETARG_INT32(0);
      keysize = PG_GETARG_INT32(1);
      
      /* key size has to be between 1 and 4, bitmaps between 1 and 2048 */
      if ((keysize < 1) || (keysize > MAX_KEYSIZE)) {
          elog(ERROR, "key size has to be between 1 and %d", MAX_KEYSIZE);
      } else if ((bitmaps < 1) || (bitmaps > MAX_BITMAPS)) {
          elog(ERROR, "number of bitmaps has to be between 1 and %d", MAX_BITMAPS);
      }
      
      PG_RETURN_INT32(pcsa_get_size(bitmaps, keysize));      
}

Datum
pcsa_length(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT32(VARSIZE((PCSACounter)PG_GETARG_BYTEA_P(0)));
}

Datum
pcsa_reset(PG_FUNCTION_ARGS)
{
	pcsa_reset_internal(((PCSACounter)PG_GETARG_BYTEA_P(0)));
	PG_RETURN_VOID();
}


/*
 *		byteain			- converts from printable representation of byte array
 *
 *		Non-printable characters must be passed as '\nnn' (octal) and are
 *		converted to internal form.  '\' must be passed as '\\'.
 *		ereport(ERROR, ...) if bad form.
 *
 *		BUGS:
 *				The input is scanned twice.
 *				The error checking of input is minimal.
 */
Datum
pcsa_in(PG_FUNCTION_ARGS)
{
	char	   *inputText = PG_GETARG_CSTRING(0);
	char	   *tp;
	char	   *rp;
	int			bc;
	bytea	   *result;

	/* Recognize hex input */
	if (inputText[0] == '\\' && inputText[1] == 'x')
	{
		size_t		len = strlen(inputText);

		bc = (len - 2) / 2 + VARHDRSZ;	/* maximum possible length */
		result = palloc(bc);
		bc = hex_decode(inputText + 2, len - 2, VARDATA(result));
		SET_VARSIZE(result, bc + VARHDRSZ);		/* actual length */

		PG_RETURN_BYTEA_P(result);
	}

	/* Else, it's the traditional escaped style */
	for (bc = 0, tp = inputText; *tp != '\0'; bc++)
	{
		if (tp[0] != '\\')
			tp++;
		else if ((tp[0] == '\\') &&
				 (tp[1] >= '0' && tp[1] <= '3') &&
				 (tp[2] >= '0' && tp[2] <= '7') &&
				 (tp[3] >= '0' && tp[3] <= '7'))
			tp += 4;
		else if ((tp[0] == '\\') &&
				 (tp[1] == '\\'))
			tp += 2;
		else
		{
			/*
			 * one backslash, not followed by another or ### valid octal
			 */
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					 errmsg("invalid input syntax for type bytea")));
		}
	}

	bc += VARHDRSZ;

	result = (bytea *) palloc(bc);
	SET_VARSIZE(result, bc);

	tp = inputText;
	rp = VARDATA(result);
	while (*tp != '\0')
	{
		if (tp[0] != '\\')
			*rp++ = *tp++;
		else if ((tp[0] == '\\') &&
				 (tp[1] >= '0' && tp[1] <= '3') &&
				 (tp[2] >= '0' && tp[2] <= '7') &&
				 (tp[3] >= '0' && tp[3] <= '7'))
		{
			bc = VAL(tp[1]);
			bc <<= 3;
			bc += VAL(tp[2]);
			bc <<= 3;
			*rp++ = bc + VAL(tp[3]);

			tp += 4;
		}
		else if ((tp[0] == '\\') &&
				 (tp[1] == '\\'))
		{
			*rp++ = '\\';
			tp += 2;
		}
		else
		{
			/*
			 * We should never get here. The first pass should not allow it.
			 */
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					 errmsg("invalid input syntax for type bytea")));
		}
	}

	PG_RETURN_BYTEA_P(result);
}

/*
 *		byteaout		- converts to printable representation of byte array
 *
 *		In the traditional escaped format, non-printable characters are
 *		printed as '\nnn' (octal) and '\' as '\\'.
 */
Datum
pcsa_out(PG_FUNCTION_ARGS)
{
	bytea	   *vlena = PG_GETARG_BYTEA_PP(0);
	char	   *result;
	char	   *rp;

	if (bytea_output == BYTEA_OUTPUT_HEX)
	{
		/* Print hex format */
		rp = result = palloc(VARSIZE_ANY_EXHDR(vlena) * 2 + 2 + 1);
		*rp++ = '\\';
		*rp++ = 'x';
		rp += hex_encode(VARDATA_ANY(vlena), VARSIZE_ANY_EXHDR(vlena), rp);
	}
	else if (bytea_output == BYTEA_OUTPUT_ESCAPE)
	{
		/* Print traditional escaped format */
		char	   *vp;
		int			len;
		int			i;

		len = 1;				/* empty string has 1 char */
		vp = VARDATA_ANY(vlena);
		for (i = VARSIZE_ANY_EXHDR(vlena); i != 0; i--, vp++)
		{
			if (*vp == '\\')
				len += 2;
			else if ((unsigned char) *vp < 0x20 || (unsigned char) *vp > 0x7e)
				len += 4;
			else
				len++;
		}
		rp = result = (char *) palloc(len);
		vp = VARDATA_ANY(vlena);
		for (i = VARSIZE_ANY_EXHDR(vlena); i != 0; i--, vp++)
		{
			if (*vp == '\\')
			{
				*rp++ = '\\';
				*rp++ = '\\';
			}
			else if ((unsigned char) *vp < 0x20 || (unsigned char) *vp > 0x7e)
			{
				int			val;	/* holds unprintable chars */

				val = *vp;
				rp[0] = '\\';
				rp[3] = DIG(val & 07);
				val >>= 3;
				rp[2] = DIG(val & 07);
				val >>= 3;
				rp[1] = DIG(val & 03);
				rp += 4;
			}
			else
				*rp++ = *vp;
		}
	}
	else
	{
		elog(ERROR, "unrecognized bytea_output setting: %d",
			 bytea_output);
		rp = result = NULL;		/* keep compiler quiet */
	}
	*rp = '\0';
	PG_RETURN_CSTRING(result);
}

/*
 *		bytearecv			- converts external binary format to bytea
 */
Datum
pcsa_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	bytea	   *result;
	int			nbytes;

	nbytes = buf->len - buf->cursor;
	result = (bytea *) palloc(nbytes + VARHDRSZ);
	SET_VARSIZE(result, nbytes + VARHDRSZ);
	pq_copymsgbytes(buf, VARDATA(result), nbytes);
	PG_RETURN_BYTEA_P(result);
}

/*
 *		byteasend			- converts bytea to binary format
 *
 * This is a special case: just copy the input...
 */
Datum
pcsa_send(PG_FUNCTION_ARGS)
{
	bytea	   *vlena = PG_GETARG_BYTEA_P_COPY(0);

	PG_RETURN_BYTEA_P(vlena);
}