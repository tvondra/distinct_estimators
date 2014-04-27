
-- merges the second estimator into the first one
CREATE FUNCTION pcsa_merge(estimator1 pcsa_estimator, estimator2 pcsa_estimator) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_merge_simple'
     LANGUAGE C;

-- merges the second estimator into the first one
CREATE FUNCTION pcsa_merge_agg(estimator1 pcsa_estimator, estimator2 pcsa_estimator) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_merge_agg'
     LANGUAGE C;

-- build the counter(s), but does not perform the final estimation (i.e. can be used to pre-aggregate data)
-- parameters: item, nbitmaps, keysize
CREATE AGGREGATE pcsa_accum(anyelement, int, int)
(
    sfunc = pcsa_add_item_agg,
    stype = pcsa_estimator
);

-- parameters: item
CREATE AGGREGATE pcsa_accum(anyelement)
(
    sfunc = pcsa_add_item_agg2,
    stype = pcsa_estimator
);

-- merges all the counters into just a single one (e.g. after running pcsa_accum)
-- parameters: estimator 
CREATE AGGREGATE pcsa_merge(pcsa_estimator)
(
    sfunc = pcsa_merge_agg,
    stype = pcsa_estimator
);

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = pcsa_get_estimate,
    RIGHTARG = pcsa_estimator
);

-- merges two estimators into a new one
CREATE OPERATOR || (
    PROCEDURE = pcsa_merge,
    LEFTARG  = pcsa_estimator,
    RIGHTARG = pcsa_estimator,
    COMMUTATOR = ||
);
