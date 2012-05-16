-- LogLog SAMPLING ESTIMATOR

-- LogLog counter (shell type)
CREATE TYPE loglog_estimator;

-- get estimator size for the requested number of bitmaps / key size
CREATE FUNCTION loglog_size(nbitmaps int, keysize int) RETURNS int
     AS 'MODULE_PATHNAME', 'loglog_size'
     LANGUAGE C;

-- creates a new LogLog estimator with a given number of bitmaps / key size
-- an estimator with 32 bitmaps and keysize 3 usually gives reasonable results
CREATE FUNCTION loglog_init(nbitmaps int, keysize int) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION loglog_add_item(counter loglog_estimator, item text) RETURNS void
     AS 'MODULE_PATHNAME', 'loglog_add_item_text'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION loglog_add_item(counter loglog_estimator, item int) RETURNS void
     AS 'MODULE_PATHNAME', 'loglog_add_item_int'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION loglog_get_estimate(counter loglog_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'loglog_get_estimate'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION loglog_reset(counter loglog_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'loglog_reset'
     LANGUAGE C STRICT;

-- length of the estimator (about the same as loglog_size with existing estimator)
CREATE FUNCTION length(counter loglog_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'loglog_length'
     LANGUAGE C STRICT;

/* functions for aggregate functions */

CREATE FUNCTION loglog_add_item_agg(counter loglog_estimator, item text, errorRate real, ndistinct int) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_add_item_agg_text'
     LANGUAGE C;

CREATE FUNCTION loglog_add_item_agg(counter loglog_estimator, item int, errorRate real, ndistinct int) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_add_item_agg_int'
     LANGUAGE C;

CREATE FUNCTION loglog_add_item_agg2(counter loglog_estimator, text) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_add_item_agg2_text'
     LANGUAGE C;

CREATE FUNCTION loglog_add_item_agg2(counter loglog_estimator, int) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_add_item_agg2_int'
     LANGUAGE C;

/* input/output functions */

CREATE FUNCTION loglog_in(value cstring) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_in'
     LANGUAGE C STRICT;

CREATE FUNCTION loglog_out(counter loglog_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'loglog_out'
     LANGUAGE C STRICT;

-- actual LogLog counter data type
CREATE TYPE loglog_estimator (
    INPUT = loglog_in,
    OUTPUT = loglog_out,
    LIKE  = bytea
);

-- LogLog based aggregate
-- items / error rate / number of items
CREATE AGGREGATE loglog_distinct(text, real, int)
(
    sfunc = loglog_add_item_agg,
    stype = loglog_estimator,
    finalfunc = loglog_get_estimate
);

CREATE AGGREGATE loglog_distinct(int, real, int)
(
    sfunc = loglog_add_item_agg,
    stype = loglog_estimator,
    finalfunc = loglog_get_estimate
);

CREATE AGGREGATE loglog_distinct(text)
(
    sfunc = loglog_add_item_agg2,
    stype = loglog_estimator,
    finalfunc = loglog_get_estimate
);

CREATE AGGREGATE loglog_distinct(int)
(
    sfunc = loglog_add_item_agg2,
    stype = loglog_estimator,
    finalfunc = loglog_get_estimate
);
