-- ADAPTIVE SAMPLING ESTIMATOR

-- shell type
CREATE TYPE pcsa_estimator;

-- get estimator size for the requested error rate / item size
CREATE FUNCTION pcsa_size(int, int) RETURNS int
     AS 'MODULE_PATHNAME', 'pcsa_size'
     LANGUAGE C;

-- creates a new adaptive estimator with a given error / item size
CREATE FUNCTION pcsa_init(int, int) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION pcsa_add_item(pcsa_estimator, text) RETURNS void
     AS 'MODULE_PATHNAME', 'pcsa_add_item_text'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION pcsa_add_item(pcsa_estimator, int) RETURNS void
     AS 'MODULE_PATHNAME', 'pcsa_add_item_int'
     LANGUAGE C;

CREATE FUNCTION pcsa_add_item_agg(pcsa_estimator, text, integer, integer) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_add_item_agg_text'
     LANGUAGE C;

CREATE FUNCTION pcsa_add_item_agg(pcsa_estimator, int, integer, integer) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_add_item_agg_int'
     LANGUAGE C;

CREATE FUNCTION pcsa_add_item_agg2(pcsa_estimator, text) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_add_item_agg2_text'
     LANGUAGE C;

CREATE FUNCTION pcsa_add_item_agg2(pcsa_estimator, int) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_add_item_agg2_int'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION pcsa_get_estimate(pcsa_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'pcsa_get_estimate'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION pcsa_reset(pcsa_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'pcsa_reset'
     LANGUAGE C;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION length(pcsa_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'pcsa_length'
     LANGUAGE C STRICT;

CREATE FUNCTION pcsa_in(cstring) RETURNS pcsa_estimator
     AS 'MODULE_PATHNAME', 'pcsa_in'
     LANGUAGE C STRICT;

CREATE FUNCTION pcsa_out(pcsa_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'pcsa_out'
     LANGUAGE C STRICT;

-- data type for the adaptive-sampling based distinct estimator
CREATE TYPE pcsa_estimator (
    INPUT = pcsa_in,
    OUTPUT = pcsa_out,
    LIKE  = bytea
);

-- adaptive based aggregate
-- items / error rate / number of items
CREATE AGGREGATE pcsa_distinct(text, int, int)
(
    sfunc = pcsa_add_item_agg,
    stype = pcsa_estimator,
    finalfunc = pcsa_get_estimate
);

CREATE AGGREGATE pcsa_distinct(int, int, int)
(
    sfunc = pcsa_add_item_agg,
    stype = pcsa_estimator,
    finalfunc = pcsa_get_estimate
);

CREATE AGGREGATE pcsa_distinct(text)
(
    sfunc = pcsa_add_item_agg2,
    stype = pcsa_estimator,
    finalfunc = pcsa_get_estimate
);

CREATE AGGREGATE pcsa_distinct(int)
(
    sfunc = pcsa_add_item_agg2,
    stype = pcsa_estimator,
    finalfunc = pcsa_get_estimate
);
