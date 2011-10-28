-- ADAPTIVE SAMPLING ESTIMATOR

-- shell type
CREATE TYPE probabilistic_estimator;

-- get estimator size for the requested error rate / item size
CREATE FUNCTION probabilistic_size(int, int) RETURNS int
     AS 'MODULE_PATHNAME', 'probabilistic_size'
     LANGUAGE C;

-- creates a new adaptive estimator with a given error / item size
CREATE FUNCTION probabilistic_init(int, int) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION probabilistic_add_item(probabilistic_estimator, text) RETURNS void
     AS 'MODULE_PATHNAME', 'probabilistic_add_item_text'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION probabilistic_add_item(probabilistic_estimator, int) RETURNS void
     AS 'MODULE_PATHNAME', 'probabilistic_add_item_int'
     LANGUAGE C;

CREATE FUNCTION probabilistic_add_item_agg(probabilistic_estimator, text, integer, integer) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_add_item_agg_text'
     LANGUAGE C;

CREATE FUNCTION probabilistic_add_item_agg(probabilistic_estimator, int, integer, integer) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_add_item_agg_int'
     LANGUAGE C;

CREATE FUNCTION probabilistic_add_item_agg2(probabilistic_estimator, text) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_add_item_agg2_text'
     LANGUAGE C;

CREATE FUNCTION probabilistic_add_item_agg2(probabilistic_estimator, int) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_add_item_agg2_int'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION probabilistic_get_estimate(probabilistic_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'probabilistic_get_estimate'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION probabilistic_reset(probabilistic_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'probabilistic_reset'
     LANGUAGE C;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION length(probabilistic_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'probabilistic_length'
     LANGUAGE C STRICT;

CREATE FUNCTION probabilistic_in(cstring) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_in'
     LANGUAGE C STRICT;

CREATE FUNCTION probabilistic_out(probabilistic_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'probabilistic_out'
     LANGUAGE C STRICT;

-- data type for the adaptive-sampling based distinct estimator
CREATE TYPE probabilistic_estimator (
    INPUT = probabilistic_in,
    OUTPUT = probabilistic_out,
    LIKE  = bytea
);

-- adaptive based aggregate
-- items / error rate / number of items
CREATE AGGREGATE probabilistic_distinct(text, int, int)
(
    sfunc = probabilistic_add_item_agg,
    stype = probabilistic_estimator,
    finalfunc = probabilistic_get_estimate
);

CREATE AGGREGATE probabilistic_distinct(int, int, int)
(
    sfunc = probabilistic_add_item_agg,
    stype = probabilistic_estimator,
    finalfunc = probabilistic_get_estimate
);

CREATE AGGREGATE probabilistic_distinct(text)
(
    sfunc = probabilistic_add_item_agg2,
    stype = probabilistic_estimator,
    finalfunc = probabilistic_get_estimate
);

CREATE AGGREGATE probabilistic_distinct(int)
(
    sfunc = probabilistic_add_item_agg2,
    stype = probabilistic_estimator,
    finalfunc = probabilistic_get_estimate
);
