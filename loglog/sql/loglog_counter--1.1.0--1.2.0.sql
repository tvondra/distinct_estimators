
-- merges the second estimator into the first one
CREATE FUNCTION loglog_merge(estimator1 loglog_estimator, estimator2 loglog_estimator) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_merge_simple'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION loglog_merge_agg(estimator1 loglog_estimator, estimator2 loglog_estimator) RETURNS loglog_estimator
     AS 'MODULE_PATHNAME', 'loglog_merge_agg'
     LANGUAGE C;

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
