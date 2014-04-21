-- HyperLogLog 

-- HyperLogLog counter (shell type)
CREATE TYPE hyperloglog_estimator;

-- get estimator size for the requested number of bitmaps / key size
CREATE FUNCTION hyperloglog_size(error_rate real) RETURNS int
     AS 'MODULE_PATHNAME', 'hyperloglog_size'
     LANGUAGE C;

-- creates a new LogLog estimator with a given number of bitmaps / key size
-- an estimator with 32 bitmaps and keysize 3 usually gives reasonable results
CREATE FUNCTION hyperloglog_init(error_rate real) RETURNS hyperloglog_estimator
     AS 'MODULE_PATHNAME', 'hyperloglog_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION hyperloglog_add_item(counter hyperloglog_estimator, item anyelement) RETURNS void
     AS 'MODULE_PATHNAME', 'hyperloglog_add_item'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION hyperloglog_get_estimate(counter hyperloglog_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'hyperloglog_get_estimate'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION hyperloglog_reset(counter hyperloglog_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'hyperloglog_reset'
     LANGUAGE C STRICT;

-- length of the estimator (about the same as hyperloglog_size with existing estimator)
CREATE FUNCTION length(counter hyperloglog_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'hyperloglog_length'
     LANGUAGE C STRICT;

/* functions for aggregate functions */

CREATE FUNCTION hyperloglog_add_item_agg(counter hyperloglog_estimator, item anyelement, error_rate real) RETURNS hyperloglog_estimator
     AS 'MODULE_PATHNAME', 'hyperloglog_add_item_agg'
     LANGUAGE C;

CREATE FUNCTION hyperloglog_add_item_agg2(counter hyperloglog_estimator, item anyelement) RETURNS hyperloglog_estimator
     AS 'MODULE_PATHNAME', 'hyperloglog_add_item_agg2'
     LANGUAGE C;

/* input/output functions */

CREATE FUNCTION hyperloglog_in(value cstring) RETURNS hyperloglog_estimator
     AS 'MODULE_PATHNAME', 'hyperloglog_in'
     LANGUAGE C STRICT;

CREATE FUNCTION hyperloglog_out(counter hyperloglog_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'hyperloglog_out'
     LANGUAGE C STRICT;

-- actual LogLog counter data type
CREATE TYPE hyperloglog_estimator (
    INPUT = hyperloglog_in,
    OUTPUT = hyperloglog_out,
    LIKE  = bytea
);

-- LogLog based aggregate (item, error rate)
CREATE AGGREGATE hyperloglog_distinct(anyelement, real)
(
    sfunc = hyperloglog_add_item_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_get_estimate
);

-- LogLog based aggregate (item)
CREATE AGGREGATE hyperloglog_distinct(anyelement)
(
    sfunc = hyperloglog_add_item_agg2,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_get_estimate
);
