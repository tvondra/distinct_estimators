
-- merges the estimators into a new copy
CREATE FUNCTION probabilistic_merge(estimator1 probabilistic_estimator, estimator2 probabilistic_estimator) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_merge_simple'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION probabilistic_merge_agg(estimator1 probabilistic_estimator, estimator2 probabilistic_estimator) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_merge_agg'
     LANGUAGE C;

-- build the counter(s), but does not perform the final estimation (i.e. can be used to pre-aggregate data)
-- parameters: item, nbytes, nsalts
CREATE AGGREGATE probabilistic_accum(anyelement, int, int)
(
    sfunc = probabilistic_add_item_agg,
    stype = probabilistic_estimator
);

-- parameters: item
CREATE AGGREGATE probabilistic_accum(anyelement)
(
    sfunc = probabilistic_add_item_agg2,
    stype = probabilistic_estimator
);

-- merges all the counters into just a single one (e.g. after running probabilistic_accum)
-- parameters: estimator
CREATE AGGREGATE probabilistic_merge(probabilistic_estimator)
(
    sfunc = probabilistic_merge_agg,
    stype = probabilistic_estimator
);

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = probabilistic_get_estimate,
    RIGHTARG = probabilistic_estimator
);

-- merges two estimators into a new one
CREATE OPERATOR || (
    PROCEDURE = probabilistic_merge,
    LEFTARG  = probabilistic_estimator,
    RIGHTARG = probabilistic_estimator,
    COMMUTATOR = ||
);
