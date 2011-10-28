#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "postgres.h"
#include "fmgr.h"
#include "pcsa.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define VAL(CH)         ((CH) - '0')
#define DIG(VAL)        ((VAL) + '0')

PG_FUNCTION_INFO_V1(pcsa_add_item_text);
PG_FUNCTION_INFO_V1(pcsa_add_item_int);

PG_FUNCTION_INFO_V1(pcsa_add_item_agg_text);
PG_FUNCTION_INFO_V1(pcsa_add_item_agg_int);

PG_FUNCTION_INFO_V1(pcsa_add_item_agg2_text);
PG_FUNCTION_INFO_V1(pcsa_add_item_agg2_int);

PG_FUNCTION_INFO_V1(pcsa_get_estimate);
PG_FUNCTION_INFO_V1(pcsa_size);
PG_FUNCTION_INFO_V1(pcsa_init);
PG_FUNCTION_INFO_V1(pcsa_reset);
PG_FUNCTION_INFO_V1(pcsa_in);
PG_FUNCTION_INFO_V1(pcsa_out);
PG_FUNCTION_INFO_V1(pcsa_rect);
PG_FUNCTION_INFO_V1(pcsa_send);
PG_FUNCTION_INFO_V1(pcsa_length);

Datum pcsa_add_item_text(PG_FUNCTION_ARGS);
Datum pcsa_add_item_int(PG_FUNCTION_ARGS);

Datum pcsa_add_item_agg_text(PG_FUNCTION_ARGS);
Datum pcsa_add_item_agg_int(PG_FUNCTION_ARGS);

Datum pcsa_add_item_agg2_text(PG_FUNCTION_ARGS);
Datum pcsa_add_item_agg2_int(PG_FUNCTION_ARGS);

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
pcsa_add_item_text(PG_FUNCTION_ARGS)
{

    PCSACounter pcsa;
    text * item;
    
    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if ((! PG_ARGISNULL(0)) && (! PG_ARGISNULL(1))) {

      pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
      
      /* get the new item */
      item = PG_GETARG_TEXT_P(1);
    
      /* in-place update works only if executed as aggregate */
      pcsa_add_element_text(pcsa, VARDATA(item), VARSIZE(item) - VARHDRSZ);
      
    } else if (PG_ARGISNULL(0)) {
        elog(ERROR, "s-bitmap counter must not be NULL");
    }
    
    PG_RETURN_VOID();

}

Datum
pcsa_add_item_int(PG_FUNCTION_ARGS)
{

    PCSACounter pcsa;
    int item;
    
    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if ((! PG_ARGISNULL(0)) && (! PG_ARGISNULL(1))) {

        pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
        
        /* get the new item */
        item = PG_GETARG_INT32(1);
        
        /* in-place update works only if executed as aggregate */
        pcsa_add_element_int(pcsa, item);
      
    } else if (PG_ARGISNULL(0)) {
        elog(ERROR, "s-bitmap counter must not be NULL");
    }
    
    PG_RETURN_VOID();

}

Datum
pcsa_add_item_agg_text(PG_FUNCTION_ARGS)
{
	
    PCSACounter pcsa;
    text * item;
    int  bitmaps; /* number of bitmaps */
    int  keysize; /* keysize */
  
    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0)) {
      bitmaps = PG_GETARG_INT32(2);
      keysize = PG_GETARG_INT32(3);
      pcsa = pcsa_create(bitmaps, keysize);
    } else {
      pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
    }
      
    /* get the new item */
    item = PG_GETARG_TEXT_P(1);
    
    /* in-place update works only if executed as aggregate */
    pcsa_add_element_text(pcsa, VARDATA(item), VARSIZE(item) - VARHDRSZ);
    
    /* return the updated bytea */
    PG_RETURN_BYTEA_P(pcsa);
    
}

Datum
pcsa_add_item_agg_int(PG_FUNCTION_ARGS)
{
    
    PCSACounter pcsa;
    int item;
    int  bitmaps; /* number of bitmaps */
    int  keysize; /* keysize */
  
    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0)) {
      bitmaps = PG_GETARG_INT32(2);
      keysize = PG_GETARG_INT32(3);
      pcsa = pcsa_create(bitmaps, keysize);
    } else {
      pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
    }
      
    /* get the new item */
    item = PG_GETARG_INT32(1);
    
    /* in-place update works only if executed as aggregate */
    pcsa_add_element_int(pcsa, item);
    
    /* return the updated bytea */
    PG_RETURN_BYTEA_P(pcsa);
    
}

Datum
pcsa_add_item_agg2_text(PG_FUNCTION_ARGS)
{
    
    PCSACounter pcsa;
    text * item;
  
    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0)) {
      pcsa = pcsa_create(64, 4);
    } else {
      pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
    }
      
    /* get the new item */
    item = PG_GETARG_TEXT_P(1);
    
    /* in-place update works only if executed as aggregate */
    pcsa_add_element_text(pcsa, VARDATA(item), VARSIZE(item) - VARHDRSZ);
    
    /* return the updated bytea */
    PG_RETURN_BYTEA_P(pcsa);
    
}

Datum
pcsa_add_item_agg2_int(PG_FUNCTION_ARGS)
{
    
    PCSACounter pcsa;
    int item;
  
    /* is the counter created (if not, create it - error 1%, 10mil items) */
    if (PG_ARGISNULL(0)) {
      pcsa = pcsa_create(64, 4);
    } else {
      pcsa = (PCSACounter)PG_GETARG_BYTEA_P(0);
    }
      
    /* get the new item */
    item = PG_GETARG_INT32(1);
    
    /* in-place update works only if executed as aggregate */
    pcsa_add_element_int(pcsa, item);
    
    /* return the updated bytea */
    PG_RETURN_BYTEA_P(pcsa);
    
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
      
      pcsa = pcsa_create(bitmaps, keysize);
      
      PG_RETURN_BYTEA_P(pcsa);
}

Datum
pcsa_size(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT32(0);
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