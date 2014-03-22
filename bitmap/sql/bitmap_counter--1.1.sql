-- BITMAP SAMPLING ESTIMATOR

-- bitmap estimator (shell type)
CREATE TYPE bitmap_estimator;

-- get estimator size for the requested error rate / number of distinct items
CREATE FUNCTION bitmap_size(error_rate real, ndistinct int) RETURNS int
     AS 'MODULE_PATHNAME', 'bitmap_size'
     LANGUAGE C;

-- creates a new bitmap estimator with a given error / number of distinct items
CREATE FUNCTION bitmap_init(error_rate real, ndistinct int) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION bitmap_add_item(counter bitmap_estimator, item anyelement) RETURNS void
     AS 'MODULE_PATHNAME', 'bitmap_add_item'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION bitmap_get_estimate(counter bitmap_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'bitmap_get_estimate'
     LANGUAGE C STRICT;

-- get error rate used when creating the estimator (as a real number)
CREATE FUNCTION bitmap_get_error(counter bitmap_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'bitmap_get_error'
     LANGUAGE C STRICT;

-- get expected number of distinct values used when creating the estimator (int)
CREATE FUNCTION bitmap_get_ndistinct(counter bitmap_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'bitmap_get_ndistinct'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION bitmap_reset(counter bitmap_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'bitmap_reset'
     LANGUAGE C STRICT;

-- length of the estimator (about the same as adaptive_size with existing estimator)
CREATE FUNCTION length(counter bitmap_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'bitmap_length'
     LANGUAGE C STRICT;

/* function for aggregate functions */

CREATE FUNCTION bitmap_add_item_agg(counter bitmap_estimator, item anyelement, error_rate real, ndistinct integer) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_add_item_agg'
     LANGUAGE C;

CREATE FUNCTION bitmap_add_item_agg2(counter bitmap_estimator, item anyelement) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_add_item_agg2'
     LANGUAGE C;

/* input/output function */

CREATE FUNCTION bitmap_in(value cstring) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_in'
     LANGUAGE C STRICT;

CREATE FUNCTION bitmap_out(counter bitmap_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'bitmap_out'
     LANGUAGE C STRICT;

-- data type for the s-bitmap based distinct estimator
CREATE TYPE bitmap_estimator (
    INPUT = bitmap_in,
    OUTPUT = bitmap_out,
    LIKE  = bytea
);

-- s-bitmap based aggregate
-- items / error rate / number of distinct items
CREATE AGGREGATE bitmap_distinct(anyelement, real, int)
(
    sfunc = bitmap_add_item_agg,
    stype = bitmap_estimator,
    finalfunc = bitmap_get_estimate
);

CREATE AGGREGATE bitmap_distinct(anyelement)
(
    sfunc = bitmap_add_item_agg2,
    stype = bitmap_estimator,
    finalfunc = bitmap_get_estimate
);
