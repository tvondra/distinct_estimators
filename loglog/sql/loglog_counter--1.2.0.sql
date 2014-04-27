-- LogLog 

-- LogLog counter (shell type)
CREATE TYPE loglog_estimator;

-- get estimator size for the requested error rate
CREATE FUNCTION loglog_size(errorRate real) RETURNS int
     AS 'MODULE_PATHNAME', 'loglog_size'
     LANGUAGE C;

-- creates a new LogLog estimator with a given a desired error rate limit
CREATE FUNCTION loglog_init(errorRate real) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_init'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION loglog_merge(estimator1 loglog_estimator, estimator2 loglog_estimator) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_merge_simple'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION loglog_merge_agg(estimator1 loglog_estimator, estimator2 loglog_estimator) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_merge_agg'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION loglog_add_item(counter loglog_estimator, item anyelement) RETURNS void
     AS 'MODULE_PATHNAME', 'loglog_add_item'
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

CREATE FUNCTION loglog_add_item_agg(counter loglog_estimator, item anyelement, errorRate real) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_add_item_agg'
     LANGUAGE C;

CREATE FUNCTION loglog_add_item_agg2(counter loglog_estimator, item anyelement) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_add_item_agg2'
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

-- LogLog based aggregate (item, error rate)
CREATE AGGREGATE loglog_distinct(anyelement, real)
(
    sfunc = loglog_add_item_agg,
    stype = loglog_estimator,
    finalfunc = loglog_get_estimate
);

-- LogLog based aggregate (item)
CREATE AGGREGATE loglog_distinct(anyelement)
(
    sfunc = loglog_add_item_agg2,
    stype = loglog_estimator,
    finalfunc = loglog_get_estimate
);

-- build the counter(s), but does not perform the final estimation (i.e. can be used to pre-aggregate data)
CREATE AGGREGATE loglog_accum(anyelement, real)
(
    sfunc = loglog_add_item_agg,
    stype = loglog_estimator
);

CREATE AGGREGATE loglog_accum(anyelement)
(
    sfunc = loglog_add_item_agg2,
    stype = loglog_estimator
);

-- merges all the counters into just a single one (e.g. after running loglog_accum)
CREATE AGGREGATE loglog_merge(loglog_estimator)
(
    sfunc = loglog_merge_agg,
    stype = loglog_estimator
);

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = loglog_get_estimate,
    RIGHTARG = loglog_estimator
);

-- merges two estimators into a new one
CREATE OPERATOR || (
    PROCEDURE = loglog_merge,
    LEFTARG  = loglog_estimator,
    RIGHTARG = loglog_estimator,
    COMMUTATOR = ||
);
