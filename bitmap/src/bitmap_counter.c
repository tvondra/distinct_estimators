#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "postgres.h"
#include "fmgr.h"
#include "bitmap.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define VAL(CH)			((CH) - '0')
#define DIG(VAL)		((VAL) + '0')

#define DEFAULT_ERROR       0.025
#define DEFAULT_NDISTINCT   1000000

PG_FUNCTION_INFO_V1(bitmap_add_item);
PG_FUNCTION_INFO_V1(bitmap_add_item_agg);
PG_FUNCTION_INFO_V1(bitmap_add_item_agg2);

PG_FUNCTION_INFO_V1(bitmap_get_estimate);
PG_FUNCTION_INFO_V1(bitmap_get_ndistinct);
PG_FUNCTION_INFO_V1(bitmap_size);
PG_FUNCTION_INFO_V1(bitmap_init);
PG_FUNCTION_INFO_V1(bitmap_get_error);
PG_FUNCTION_INFO_V1(bitmap_reset);
PG_FUNCTION_INFO_V1(bitmap_in);
PG_FUNCTION_INFO_V1(bitmap_out);
PG_FUNCTION_INFO_V1(bitmap_rect);
PG_FUNCTION_INFO_V1(bitmap_send);
PG_FUNCTION_INFO_V1(bitmap_length);

Datum bitmap_add_item(PG_FUNCTION_ARGS);
Datum bitmap_add_item_agg(PG_FUNCTION_ARGS);
Datum bitmap_add_item_agg2(PG_FUNCTION_ARGS);

Datum bitmap_get_estimate(PG_FUNCTION_ARGS);
Datum bitmap_get_ndistinct(PG_FUNCTION_ARGS);
Datum bitmap_size(PG_FUNCTION_ARGS);
Datum bitmap_init(PG_FUNCTION_ARGS);
Datum bitmap_get_error(PG_FUNCTION_ARGS);
Datum bitmap_reset(PG_FUNCTION_ARGS);
Datum bitmap_in(PG_FUNCTION_ARGS);
Datum bitmap_out(PG_FUNCTION_ARGS);
Datum bitmap_recv(PG_FUNCTION_ARGS);
Datum bitmap_send(PG_FUNCTION_ARGS);
Datum bitmap_length(PG_FUNCTION_ARGS);

Datum
bitmap_add_item(PG_FUNCTION_ARGS)
{

    BitmapCounter bitmap_counter;

    /* requires the estimator to be already created */
    if (PG_ARGISNULL(0))
        elog(ERROR, "bitmap counter must not be NULL");

    /* if the element is not NULL, add it to the estimator (i.e. skip NULLs) */
    if (! PG_ARGISNULL(1)) {

        Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
        Datum       element = PG_GETARG_DATUM(1);
        int16       typlen;
        bool        typbyval;
        char        typalign;

        /* estimator (we know it's not a NULL value) */
        bitmap_counter = (BitmapCounter)PG_GETARG_BYTEA_P(0);

        /* TODO The requests for type info shouldn't be a problem (thanks to lsyscache),
         * but if it turns out to have a noticeable impact it's possible to cache that
         * between the calls (in the estimator). */

        /* get type information for the second parameter (anyelement item) */
        get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

        /* it this a varlena type, passed by reference or by value ? */
        if (typlen == -1) {
            /* varlena */
            bc_add_item(bitmap_counter, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            bc_add_item(bitmap_counter, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            bc_add_item(bitmap_counter, (char*)element, typlen);
        }

    }

    PG_RETURN_VOID();

}

Datum
bitmap_add_item_agg(PG_FUNCTION_ARGS)
{

    BitmapCounter bitmap_counter;
    float4 errorRate; /* 0 - 1, e.g. 0.01 means 1% */
    int    ndistinct; /* expected number of distinct values */

    /* info for anyelement */
    Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
    Datum       element = PG_GETARG_DATUM(1);
    int16       typlen;
    bool        typbyval;
    char        typalign;

    /* create a new estimator (with requested error rate) or reuse the existing one */
    if (PG_ARGISNULL(0)) {
        
        errorRate = PG_GETARG_FLOAT4(2);
        ndistinct = PG_GETARG_INT32(3);

        /* ndistinct has to be positive, error rate between 0 and 1 (not 0) */
        if (ndistinct < 1) {
            elog(ERROR, "ndistinct (expected number of distinct values) has to at least 1");
        } else if ((errorRate <= 0) || (errorRate > 1)) {
            elog(ERROR, "error rate has to be between 0 and 1");
        }
        
        bitmap_counter = bc_init(errorRate, ndistinct);

    } else { /* existing estimator */
        bitmap_counter = (BitmapCounter)PG_GETARG_BYTEA_P(0);
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
            bc_add_item(bitmap_counter, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            bc_add_item(bitmap_counter, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            bc_add_item(bitmap_counter, (char*)element, typlen);
        }

    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(bitmap_counter);
    
}

Datum
bitmap_add_item_agg2(PG_FUNCTION_ARGS)
{

    BitmapCounter bitmap_counter;

    /* info for anyelement */
    Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
    Datum       element = PG_GETARG_DATUM(1);
    int16       typlen;
    bool        typbyval;
    char        typalign;

    /* create a new estimator (with requested error rate) or reuse the existing one */
    if (PG_ARGISNULL(0)) {
        bitmap_counter = bc_init(DEFAULT_ERROR, DEFAULT_NDISTINCT);
    } else { /* existing estimator */
        bitmap_counter = (BitmapCounter)PG_GETARG_BYTEA_P(0);
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
            bc_add_item(bitmap_counter, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            bc_add_item(bitmap_counter, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            bc_add_item(bitmap_counter, (char*)element, typlen);
        }

    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(bitmap_counter);

}

Datum
bitmap_get_estimate(PG_FUNCTION_ARGS)
{
  
    int estimate;
    BitmapCounter bc = (BitmapCounter)PG_GETARG_BYTEA_P(0);
    
    /* in-place update works only if executed as aggregate */
    estimate = bc_estimate(bc);
    
    /* return the updated bytea */
    PG_RETURN_FLOAT4(estimate);

}

Datum
bitmap_get_ndistinct(PG_FUNCTION_ARGS)
{
  
    BitmapCounter bc = (BitmapCounter)PG_GETARG_BYTEA_P(0);
  
    /* return the updated bytea */
    PG_RETURN_INT32(bc->ndistinct);

}

Datum
bitmap_size(PG_FUNCTION_ARGS)
{

    float error;
    int ndistinct;
    float m;
    int bits, nbits, bitmapSize;
    
    error = PG_GETARG_FLOAT4(0);
    ndistinct = PG_GETARG_INT32(1);
    
    /* ndistinct has to be positive, error rate between 0 and 1 (not 0) */
    if (ndistinct < 1) {
        elog(ERROR, "ndistinct (expected number of distinct values) has to at least 1");
    } else if ((error <= 0) || (error > 1)) {
        elog(ERROR, "error rate has to be between 0 and 1");
    }

    /* compute the number of bits (see the paper "distinct counting with self-learning bitmap" page 3) */
    m = log(1 + 2*ndistinct*powf(error,2)) / log(1 + 2*powf(error,2)/(1 - powf(error,2)));

    /* how many bits do we need for index (size of the 'c') */
    bits = ceil(log(m)/log(2));
    
    /* size of the bitmap */
    nbits = (0x1 << bits);

    /* size of the bitmap (in bytes) */
    bitmapSize = nbits/sizeof(char);
    
    PG_RETURN_INT32(sizeof(BitmapCounterData) + bitmapSize - 1);

}

Datum
bitmap_init(PG_FUNCTION_ARGS)
{
    BitmapCounter bc;
    float errorRate;
    int ndistinct;
      
    errorRate = PG_GETARG_FLOAT4(0);
    ndistinct = PG_GETARG_INT32(1);
    
    /* ndistinct has to be positive, error rate between 0 and 1 (not 0) */
    if (ndistinct < 1) {
        elog(ERROR, "ndistinct (expected number of distinct values) has to at least 1");
    } else if ((errorRate <= 0) || (errorRate > 1)) {
        elog(ERROR, "error rate has to be between 0 and 1");
    }
      
    bc = bc_init(errorRate, ndistinct);
      
    PG_RETURN_BYTEA_P(bc);
}

Datum
bitmap_length(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT32(VARSIZE((BitmapCounter)PG_GETARG_BYTEA_P(0)));
}

Datum
bitmap_get_error(PG_FUNCTION_ARGS)
{

    /* return the error rate of the counter */
    PG_RETURN_FLOAT4(((BitmapCounter)PG_GETARG_BYTEA_P(0))->error);
    
}

Datum
bitmap_reset(PG_FUNCTION_ARGS)
{
	bc_reset(((BitmapCounter)PG_GETARG_BYTEA_P(0)));
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
bitmap_in(PG_FUNCTION_ARGS)
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
bitmap_out(PG_FUNCTION_ARGS)
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
bitmap_recv(PG_FUNCTION_ARGS)
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
bitmap_send(PG_FUNCTION_ARGS)
{
	bytea	   *vlena = PG_GETARG_BYTEA_P_COPY(0);

	PG_RETURN_BYTEA_P(vlena);
}