#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "postgres.h"
#include "fmgr.h"
#include "hyperloglog.h"
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

/* shoot for 10^9 distinct items and 2.5% error rate by default */
#define DEFAULT_NDISTINCT   1000000000
#define DEFAULT_ERROR       0.025

PG_FUNCTION_INFO_V1(hyperloglog_add_item);
PG_FUNCTION_INFO_V1(hyperloglog_add_item_agg);
PG_FUNCTION_INFO_V1(hyperloglog_add_item_agg2);

PG_FUNCTION_INFO_V1(hyperloglog_merge_simple);
PG_FUNCTION_INFO_V1(hyperloglog_merge_agg);
PG_FUNCTION_INFO_V1(hyperloglog_get_estimate);

PG_FUNCTION_INFO_V1(hyperloglog_size);
PG_FUNCTION_INFO_V1(hyperloglog_init);
PG_FUNCTION_INFO_V1(hyperloglog_reset);
PG_FUNCTION_INFO_V1(hyperloglog_in);
PG_FUNCTION_INFO_V1(hyperloglog_out);
PG_FUNCTION_INFO_V1(hyperloglog_rect);
PG_FUNCTION_INFO_V1(hyperloglog_send);
PG_FUNCTION_INFO_V1(hyperloglog_length);

Datum hyperloglog_add_item(PG_FUNCTION_ARGS);
Datum hyperloglog_add_item_agg(PG_FUNCTION_ARGS);
Datum hyperloglog_add_item_agg2(PG_FUNCTION_ARGS);

Datum hyperloglog_get_estimate(PG_FUNCTION_ARGS);
Datum hyperloglog_merge_simple(PG_FUNCTION_ARGS);
Datum hyperloglog_merge_agg(PG_FUNCTION_ARGS);

Datum hyperloglog_size(PG_FUNCTION_ARGS);
Datum hyperloglog_init(PG_FUNCTION_ARGS);
Datum hyperloglog_reset(PG_FUNCTION_ARGS);
Datum hyperloglog_in(PG_FUNCTION_ARGS);
Datum hyperloglog_out(PG_FUNCTION_ARGS);
Datum hyperloglog_recv(PG_FUNCTION_ARGS);
Datum hyperloglog_send(PG_FUNCTION_ARGS);
Datum hyperloglog_length(PG_FUNCTION_ARGS);

Datum
hyperloglog_add_item(PG_FUNCTION_ARGS)
{

    HyperLogLogCounter hyperloglog;

    /* requires the estimator to be already created */
    if (PG_ARGISNULL(0))
        elog(ERROR, "hyperloglog counter must not be NULL");

    /* if the element is not NULL, add it to the estimator (i.e. skip NULLs) */
    if (! PG_ARGISNULL(1)) {

        Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
        Datum       element = PG_GETARG_DATUM(1);
        int16       typlen;
        bool        typbyval;
        char        typalign;

        /* estimator (we know it's not a NULL value) */
        hyperloglog = (HyperLogLogCounter)PG_GETARG_BYTEA_P(0);

        /* TODO The requests for type info shouldn't be a problem (thanks to lsyscache),
         * but if it turns out to have a noticeable impact it's possible to cache that
         * between the calls (in the estimator). */

        /* get type information for the second parameter (anyelement item) */
        get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

        /* it this a varlena type, passed by reference or by value ? */
        if (typlen == -1) {
            /* varlena */
            hyperloglog_add_element(hyperloglog, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            hyperloglog_add_element(hyperloglog, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            hyperloglog_add_element(hyperloglog, (char*)element, typlen);
        }

    }

    PG_RETURN_VOID();

}

Datum
hyperloglog_add_item_agg(PG_FUNCTION_ARGS)
{

    HyperLogLogCounter hyperloglog;
    float errorRate; /* required error rate */

    /* info for anyelement */
    Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
    Datum       element = PG_GETARG_DATUM(1);
    int16       typlen;
    bool        typbyval;
    char        typalign;

    /* create a new estimator (with requested error rate) or reuse the existing one */
    if (PG_ARGISNULL(0)) {

        errorRate = PG_GETARG_FLOAT4(2);

        /* error rate between 0 and 1 (not 0) */
        if ((errorRate <= 0) || (errorRate > 1))
            elog(ERROR, "error rate has to be between 0 and 1");

        hyperloglog = hyperloglog_create(DEFAULT_NDISTINCT, errorRate);

    } else { /* existing estimator */
        hyperloglog = (HyperLogLogCounter)PG_GETARG_BYTEA_P(0);
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
            hyperloglog_add_element(hyperloglog, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            hyperloglog_add_element(hyperloglog, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            hyperloglog_add_element(hyperloglog, (char*)element, typlen);
        }
    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(hyperloglog);

}

Datum
hyperloglog_add_item_agg2(PG_FUNCTION_ARGS)
{

    HyperLogLogCounter hyperloglog;

    /* info for anyelement */
    Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
    Datum       element = PG_GETARG_DATUM(1);
    int16       typlen;
    bool        typbyval;
    char        typalign;

    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0)) {
      hyperloglog = hyperloglog_create(DEFAULT_NDISTINCT, DEFAULT_ERROR);
    } else {
      hyperloglog = (HyperLogLogCounter)PG_GETARG_BYTEA_P(0);
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
            hyperloglog_add_element(hyperloglog, VARDATA(element), VARSIZE(element) - VARHDRSZ);
        } else if (typbyval) {
            /* fixed-length, passed by value */
            hyperloglog_add_element(hyperloglog, (char*)&element, typlen);
        } else {
            /* fixed-length, passed by reference */
            hyperloglog_add_element(hyperloglog, (char*)element, typlen);
        }

    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(hyperloglog);

}

Datum
hyperloglog_merge_simple(PG_FUNCTION_ARGS)
{

    HyperLogLogCounter counter1 = (HyperLogLogCounter)PG_GETARG_BYTEA_P(0);
    HyperLogLogCounter counter2 = (HyperLogLogCounter)PG_GETARG_BYTEA_P(1);

    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0) && PG_ARGISNULL(1)) {
        PG_RETURN_NULL();
    } else if (PG_ARGISNULL(0)) {
        PG_RETURN_BYTEA_P(hyperloglog_copy(counter2));
    } else if (PG_ARGISNULL(1)) {
        PG_RETURN_BYTEA_P(hyperloglog_copy(counter1));
    } else {
        PG_RETURN_BYTEA_P(hyperloglog_merge(counter1, counter2, false));
    }

}

Datum
hyperloglog_merge_agg(PG_FUNCTION_ARGS)
{

    HyperLogLogCounter counter1;
    HyperLogLogCounter counter2 = (HyperLogLogCounter)PG_GETARG_BYTEA_P(1);

    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0)) {

        /* just copy the second estimator into the first one */
        counter1 = hyperloglog_copy(counter2);

    } else {

        /* ok, we already have the estimator - merge the second one into it */
        counter1 = (HyperLogLogCounter)PG_GETARG_BYTEA_P(0);

        /* perform the merge (in place) */
        counter1 = hyperloglog_merge(counter1, counter2, true);

    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(counter1);

}


Datum
hyperloglog_get_estimate(PG_FUNCTION_ARGS)
{

    int estimate;
    HyperLogLogCounter hyperloglog = (HyperLogLogCounter)PG_GETARG_BYTEA_P(0);

    /* in-place update works only if executed as aggregate */
    estimate = hyperloglog_estimate(hyperloglog);

    /* return the updated bytea */
    PG_RETURN_FLOAT4(estimate);

}

Datum
hyperloglog_init(PG_FUNCTION_ARGS)
{
      HyperLogLogCounter hyperloglog;

      float errorRate; /* required error rate */

      errorRate = PG_GETARG_FLOAT4(0);

      /* error rate between 0 and 1 (not 0) */
      if ((errorRate <= 0) || (errorRate > 1)) {
          elog(ERROR, "error rate has to be between 0 and 1");
      }

      hyperloglog = hyperloglog_create(DEFAULT_NDISTINCT, errorRate);

      PG_RETURN_BYTEA_P(hyperloglog);
}

Datum
hyperloglog_size(PG_FUNCTION_ARGS)
{

      float errorRate; /* required error rate */

      errorRate = PG_GETARG_FLOAT4(0);

      /* error rate between 0 and 1 (not 0) */
      if ((errorRate <= 0) || (errorRate > 1)) {
          elog(ERROR, "error rate has to be between 0 and 1");
      }

      PG_RETURN_INT32(hyperloglog_get_size(DEFAULT_NDISTINCT, errorRate));
}

Datum
hyperloglog_length(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT32(VARSIZE((HyperLogLogCounter)PG_GETARG_BYTEA_P(0)));
}

Datum
hyperloglog_reset(PG_FUNCTION_ARGS)
{
	hyperloglog_reset_internal(((HyperLogLogCounter)PG_GETARG_BYTEA_P(0)));
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
hyperloglog_in(PG_FUNCTION_ARGS)
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
hyperloglog_out(PG_FUNCTION_ARGS)
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
hyperloglog_recv(PG_FUNCTION_ARGS)
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
hyperloglog_send(PG_FUNCTION_ARGS)
{
	bytea	   *vlena = PG_GETARG_BYTEA_P_COPY(0);

	PG_RETURN_BYTEA_P(vlena);
}
