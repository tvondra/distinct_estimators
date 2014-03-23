-- PCSA SAMPLING ESTIMATOR

-- PCSA counter (shell type)
CREATE TYPE pcsa_estimator;

-- get estimator size for the requested number of bitmaps / key size
CREATE FUNCTION pcsa_size(nbitmaps int, keysize int) RETURNS int
     AS 'MODULE_PATHNAME', 'pcsa_size'
     LANGUAGE C;

-- creates a new pcsa estimator with a given number of bitmaps / key size
-- an estimator with 32 bitmaps and keysize 3 usually gives reasonable results
CREATE FUNCTION pcsa_init(nbitmaps int, keysize int) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION pcsa_add_item(counter pcsa_estimator, item anyelement) RETURNS void
     AS 'MODULE_PATHNAME', 'pcsa_add_item'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION pcsa_get_estimate(counter pcsa_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'pcsa_get_estimate'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION pcsa_reset(counter pcsa_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'pcsa_reset'
     LANGUAGE C STRICT;

-- length of the estimator (about the same as pcsa_size with existing estimator)
CREATE FUNCTION length(counter pcsa_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'pcsa_length'
     LANGUAGE C STRICT;

/* functions for aggregate functions */

CREATE FUNCTION pcsa_add_item_agg(counter pcsa_estimator, item anyelement, nbitmaps integer, keysize integer) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_add_item_agg'
     LANGUAGE C;

CREATE FUNCTION pcsa_add_item_agg2(counter pcsa_estimator, item anyelement) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_add_item_agg2'
     LANGUAGE C;

/* input/output functions */

CREATE FUNCTION pcsa_in(value cstring) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_in'
     LANGUAGE C STRICT;

CREATE FUNCTION pcsa_out(counter pcsa_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'pcsa_out'
     LANGUAGE C STRICT;

-- actual PCSA counter data type
CREATE TYPE pcsa_estimator (
    INPUT = pcsa_in,
    OUTPUT = pcsa_out,
    LIKE  = bytea
);

-- pcsa based aggregate
-- items / error rate / number of items
CREATE AGGREGATE pcsa_distinct(item anyelement, nbitmaps int, keysize int)
(
    sfunc = pcsa_add_item_agg,
    stype = pcsa_estimator,
    finalfunc = pcsa_get_estimate
);

CREATE AGGREGATE pcsa_distinct(item anyelement)
(
    sfunc = pcsa_add_item_agg2,
    stype = pcsa_estimator,
    finalfunc = pcsa_get_estimate
);
