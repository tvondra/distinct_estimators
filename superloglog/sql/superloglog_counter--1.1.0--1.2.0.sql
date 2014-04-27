-- merges the second estimator into the first one
CREATE FUNCTION superloglog_merge(estimator1 superloglog_estimator, estimator2 superloglog_estimator) RETURNS superloglog_estimator
     AS 'MODULE_PATHNAME', 'superloglog_merge_simple'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION superloglog_merge_agg(estimator1 superloglog_estimator, estimator2 superloglog_estimator) RETURNS superloglog_estimator
     AS 'MODULE_PATHNAME', 'superloglog_merge_agg'
     LANGUAGE C;

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
-- parameters: estimator
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
