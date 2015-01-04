-- LogLog 

-- LogLog counter (shell type)
CREATE TYPE superloglog_estimator;

-- get estimator size for the requested error rate
CREATE FUNCTION superloglog_size(error_rate real) RETURNS int
     AS '$libdir/superloglog_counter', 'superloglog_size'
     LANGUAGE C;

-- creates a new LogLog estimator with a given a desired error rate limit
CREATE FUNCTION superloglog_init(error_rate real) RETURNS superloglog_estimator
     AS '$libdir/superloglog_counter', 'superloglog_init'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION superloglog_merge(estimator1 superloglog_estimator, estimator2 superloglog_estimator) RETURNS superloglog_estimator
     AS '$libdir/superloglog_counter', 'superloglog_merge_simple'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION superloglog_merge_agg(estimator1 superloglog_estimator, estimator2 superloglog_estimator) RETURNS superloglog_estimator
     AS '$libdir/superloglog_counter', 'superloglog_merge_agg'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION superloglog_add_item(counter superloglog_estimator, item anyelement) RETURNS void
     AS '$libdir/superloglog_counter', 'superloglog_add_item'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION superloglog_get_estimate(counter superloglog_estimator) RETURNS real
     AS '$libdir/superloglog_counter', 'superloglog_get_estimate'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION superloglog_reset(counter superloglog_estimator) RETURNS void
     AS '$libdir/superloglog_counter', 'superloglog_reset'
     LANGUAGE C STRICT;

-- length of the estimator (about the same as superloglog_size with existing estimator)
CREATE FUNCTION length(counter superloglog_estimator) RETURNS int
     AS '$libdir/superloglog_counter', 'superloglog_length'
     LANGUAGE C STRICT;

/* functions for aggregate functions */

CREATE FUNCTION superloglog_add_item_agg(counter superloglog_estimator, item anyelement, error_rate real) RETURNS superloglog_estimator
     AS '$libdir/superloglog_counter', 'superloglog_add_item_agg'
     LANGUAGE C;

CREATE FUNCTION superloglog_add_item_agg2(counter superloglog_estimator, item anyelement) RETURNS superloglog_estimator
     AS '$libdir/superloglog_counter', 'superloglog_add_item_agg2'
     LANGUAGE C;

/* input/output functions */

CREATE FUNCTION superloglog_in(value cstring) RETURNS superloglog_estimator
     AS '$libdir/superloglog_counter', 'superloglog_in'
     LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION superloglog_out(counter superloglog_estimator) RETURNS cstring
     AS '$libdir/superloglog_counter', 'superloglog_out'
     LANGUAGE C IMMUTABLE STRICT;

-- actual LogLog counter data type
CREATE TYPE superloglog_estimator (
    INPUT = superloglog_in,
    OUTPUT = superloglog_out,
    LIKE  = bytea
);

-- LogLog based aggregate (item, error rate)
CREATE AGGREGATE superloglog_distinct(anyelement, real)
(
    sfunc = superloglog_add_item_agg,
    stype = superloglog_estimator,
    finalfunc = superloglog_get_estimate
);

-- LogLog based aggregate (item)
CREATE AGGREGATE superloglog_distinct(anyelement)
(
    sfunc = superloglog_add_item_agg2,
    stype = superloglog_estimator,
    finalfunc = superloglog_get_estimate
);

-- build the counter(s), but does not perform the final estimation (i.e. can be used to pre-aggregate data)
-- parameters: item, error_rate
CREATE AGGREGATE superloglog_accum(anyelement, real)
(
    sfunc = superloglog_add_item_agg,
    stype = superloglog_estimator
);

-- parameters: item
CREATE AGGREGATE superloglog_accum(anyelement)
(
    sfunc = superloglog_add_item_agg2,
    stype = superloglog_estimator
);

-- merges all the counters into just a single one (e.g. after running superloglog_accum)
CREATE AGGREGATE superloglog_merge(superloglog_estimator)
(
    sfunc = superloglog_merge_agg,
    stype = superloglog_estimator
);

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = superloglog_get_estimate,
    RIGHTARG = superloglog_estimator
);

-- merges two estimators into a new one
CREATE OPERATOR || (
    PROCEDURE = superloglog_merge,
    LEFTARG  = superloglog_estimator,
    RIGHTARG = superloglog_estimator,
    COMMUTATOR = ||
);
