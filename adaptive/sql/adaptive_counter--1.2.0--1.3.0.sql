--- drop old merge function

-- merges the two estimators, creates a new one
CREATE OR REPLACE FUNCTION adaptive_merge(estimator1 adaptive_estimator, estimator2 adaptive_estimator) RETURNS adaptive_estimator
     AS 'MODULE_PATHNAME', 'adaptive_merge_simple'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION adaptive_merge_agg(estimator1 adaptive_estimator, estimator2 adaptive_estimator) RETURNS adaptive_estimator
     AS 'MODULE_PATHNAME', 'adaptive_merge_agg'
     LANGUAGE C;

-- build the counter(s), but does not perform the final estimation (i.e. can be used to pre-aggregate data)
-- parameters: item, error_rate, ndistinct
CREATE AGGREGATE adaptive_accum(anyelement, real, int)
(
    sfunc = adaptive_add_item_agg,
    stype = adaptive_estimator
);

CREATE AGGREGATE adaptive_accum(anyelement)
(
    sfunc = adaptive_add_item_agg2,
    stype = adaptive_estimator
);

-- merges all the counters into just a single one (e.g. after running adaptive_accum)
-- parameters: estimator
CREATE AGGREGATE adaptive_merge(adaptive_estimator)
(
    sfunc = adaptive_merge_agg,
    stype = adaptive_estimator
);

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = adaptive_get_estimate,
    RIGHTARG = adaptive_estimator
);

-- merges two estimators into a new one
CREATE OPERATOR || (
    PROCEDURE = adaptive_merge,
    LEFTARG  = adaptive_estimator,
    RIGHTARG = adaptive_estimator,
    COMMUTATOR = ||
);
