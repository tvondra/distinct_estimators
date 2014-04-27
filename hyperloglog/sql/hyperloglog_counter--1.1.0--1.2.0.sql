-- merges the second estimator into the first one
CREATE FUNCTION hyperloglog_merge(estimator1 hyperloglog_estimator, estimator2 hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS 'MODULE_PATHNAME', 'hyperloglog_merge_simple'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION hyperloglog_merge_agg(estimator1 hyperloglog_estimator, estimator2 hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS 'MODULE_PATHNAME', 'hyperloglog_merge_agg'
     LANGUAGE C;

-- build the counter(s), but does not perform the final estimation (i.e. can be used to pre-aggregate data)
CREATE AGGREGATE hyperloglog_accum(anyelement, real)
(
    sfunc = hyperloglog_add_item_agg,
    stype = hyperloglog_estimator
);

CREATE AGGREGATE hyperloglog_accum(anyelement)
(
    sfunc = hyperloglog_add_item_agg2,
    stype = hyperloglog_estimator
);

-- merges all the counters into just a single one (e.g. after running hyperloglog_accum)
CREATE AGGREGATE hyperloglog_merge(hyperloglog_estimator)
(
    sfunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator
);

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = hyperloglog_get_estimate,
    RIGHTARG = hyperloglog_estimator
);

-- merges two estimators into a new one
CREATE OPERATOR || (
    PROCEDURE = hyperloglog_merge,
    LEFTARG  = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = ||
);
