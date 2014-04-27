-- build the counter(s), but does not perform the final estimation (i.e. can be used to pre-aggregate data)
-- parameters: item, error_rate, ndistinct
CREATE AGGREGATE bitmap_accum(anyelement, real, int)
(
    sfunc = bitmap_add_item_agg,
    stype = bitmap_estimator
);

-- parameters: item
CREATE AGGREGATE bitmap_accum(anyelement)
(
    sfunc = bitmap_add_item_agg2,
    stype = bitmap_estimator
);

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = bitmap_get_estimate,
    RIGHTARG = bitmap_estimator
);
