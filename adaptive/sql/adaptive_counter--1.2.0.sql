-- ADAPTIVE SAMPLING ESTIMATOR

-- adaptive_estimator data type (shell type)
CREATE TYPE adaptive_estimator;

-- get estimator size for the requested error rate / number of distinct items
CREATE FUNCTION adaptive_size(error_rate real, ndistinct int) RETURNS int
     AS 'MODULE_PATHNAME', 'adaptive_size'
     LANGUAGE C;

-- creates a new adaptive estimator with a given error / number of distinct items
CREATE FUNCTION adaptive_init(error_rate real, ndistinct int) RETURNS adaptive_estimator
     AS 'MODULE_PATHNAME', 'adaptive_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION adaptive_add_item(counter adaptive_estimator, item anyelement) RETURNS void
     AS 'MODULE_PATHNAME', 'adaptive_add_item'
     LANGUAGE C;

-- get error rate used when creating the estimator (as a real number)
CREATE FUNCTION adaptive_get_error(counter adaptive_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'adaptive_get_error'
     LANGUAGE C STRICT;

-- get expected number of distinct values used when creating the estimator (as integer)
CREATE FUNCTION adaptive_get_ndistinct(counter adaptive_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'adaptive_get_ndistinct'
     LANGUAGE C STRICT;

-- get number of distinct values used when creating the estimator (int)
CREATE FUNCTION adaptive_get_item_size(counter adaptive_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'adaptive_get_item_size'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION adaptive_reset(counter adaptive_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'adaptive_reset'
     LANGUAGE C STRICT;

-- length of the estimator (about the same as adaptive_size with existing estimator)
CREATE FUNCTION length(counter adaptive_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'adaptive_length'
     LANGUAGE C STRICT;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION adaptive_get_estimate(counter adaptive_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'adaptive_get_estimate'
     LANGUAGE C STRICT;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION adaptive_merge(adaptive_estimator, adaptive_estimator) RETURNS adaptive_estimator
     AS 'MODULE_PATHNAME', 'adaptive_merge'
     LANGUAGE C;

/* functions for the aggregates */
CREATE FUNCTION adaptive_add_item_agg(counter adaptive_estimator, item anyelement, error_rate real, ndistinct int) RETURNS adaptive_estimator
     AS 'MODULE_PATHNAME', 'adaptive_add_item_agg'
     LANGUAGE C;

CREATE FUNCTION adaptive_add_item_agg2(counter adaptive_estimator, item anyelement) RETURNS adaptive_estimator
     AS 'MODULE_PATHNAME', 'adaptive_add_item_agg2'
     LANGUAGE C;

/* input / output functions */
CREATE FUNCTION adaptive_in(value cstring) RETURNS adaptive_estimator
     AS 'MODULE_PATHNAME', 'adaptive_in'
     LANGUAGE C STRICT;

CREATE FUNCTION adaptive_out(counter adaptive_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'adaptive_out'
     LANGUAGE C STRICT;

-- data type for the adaptive-sampling based distinct estimator
CREATE TYPE adaptive_estimator (
    INPUT = adaptive_in,
    OUTPUT = adaptive_out,
    LIKE  = bytea
);

-- adaptive based aggregate (item, error rate, ndistinct)
CREATE AGGREGATE adaptive_distinct(anyelement, real, int)
(
    sfunc = adaptive_add_item_agg,
    stype = adaptive_estimator,
    finalfunc = adaptive_get_estimate
);

-- adaptive based aggregate (item)
CREATE AGGREGATE adaptive_distinct(anyelement)
(
    sfunc = adaptive_add_item_agg2,
    stype = adaptive_estimator,
    finalfunc = adaptive_get_estimate
);
