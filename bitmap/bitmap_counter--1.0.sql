-- BITMAP SAMPLING ESTIMATOR

-- shell type
CREATE TYPE bitmap_estimator;

-- get estimator size for the requested error rate / item size
CREATE FUNCTION bitmap_size(real, int) RETURNS int
     AS 'MODULE_PATHNAME', 'bitmap_size'
     LANGUAGE C;

-- creates a new bitmap estimator with a given error / item size
CREATE FUNCTION bitmap_init(real, int) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION bitmap_add_item(bitmap_estimator, text) RETURNS void
     AS 'MODULE_PATHNAME', 'bitmap_add_item_text'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION bitmap_add_item(bitmap_estimator, int) RETURNS void
     AS 'MODULE_PATHNAME', 'bitmap_add_item_int'
     LANGUAGE C;

CREATE FUNCTION bitmap_add_item_agg(bitmap_estimator, text, real, integer) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_add_item_agg_text'
     LANGUAGE C;

CREATE FUNCTION bitmap_add_item_agg(bitmap_estimator, int, real, integer) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_add_item_agg_int'
     LANGUAGE C;

CREATE FUNCTION bitmap_add_item_agg2(bitmap_estimator, text) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_add_item_agg2_text'
     LANGUAGE C;

CREATE FUNCTION bitmap_add_item_agg2(bitmap_estimator, int) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_add_item_agg2_int'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION bitmap_get_estimate(bitmap_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'bitmap_get_estimate'
     LANGUAGE C STRICT;

-- get error rate used when creating the estimator (as a real number)
CREATE FUNCTION bitmap_get_error(bitmap_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'bitmap_get_error'
     LANGUAGE C STRICT;

-- get expected number of distinct values used when creating the estimator (int)
CREATE FUNCTION bitmap_get_ndistinct(bitmap_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'bitmap_get_ndistinct'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION bitmap_reset(bitmap_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'bitmap_reset'
     LANGUAGE C;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION length(bitmap_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'bitmap_length'
     LANGUAGE C STRICT;

CREATE FUNCTION bitmap_in(cstring) RETURNS bitmap_estimator
     AS 'MODULE_PATHNAME', 'bitmap_in'
     LANGUAGE C STRICT;

CREATE FUNCTION bitmap_out(bitmap_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'bitmap_out'
     LANGUAGE C STRICT;

-- data type for the s-bitmap based distinct estimator
CREATE TYPE bitmap_estimator (
    INPUT = bitmap_in,
    OUTPUT = bitmap_out,
    LIKE  = bytea
);

-- s-bitmap based aggregate
-- items / error rate / number of items
CREATE AGGREGATE bitmap_distinct(text, real, int)
(
    sfunc = bitmap_add_item_agg,
    stype = bitmap_estimator,
    finalfunc = bitmap_get_estimate
);

CREATE AGGREGATE bitmap_distinct(int, real, int)
(
    sfunc = bitmap_add_item_agg,
    stype = bitmap_estimator,
    finalfunc = bitmap_get_estimate
);

CREATE AGGREGATE bitmap_distinct(text)
(
    sfunc = bitmap_add_item_agg2,
    stype = bitmap_estimator,
    finalfunc = bitmap_get_estimate
);

CREATE AGGREGATE bitmap_distinct(int)
(
    sfunc = bitmap_add_item_agg2,
    stype = bitmap_estimator,
    finalfunc = bitmap_get_estimate
);
