-- PROBABILISTIC COUNTING ESTIMATOR

-- probabilistic counter data type (shell type)
CREATE TYPE probabilistic_estimator;

-- get estimator size for the requested number of salts / bytes per salt
CREATE FUNCTION probabilistic_size(nbytes int, nsalts int) RETURNS int
     AS 'MODULE_PATHNAME', 'probabilistic_size'
     LANGUAGE C;

-- get estimator size for the requested number of salts / bytes per salt
CREATE FUNCTION probabilistic_init(nbytes int, nsalts int) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_init'
     LANGUAGE C;

-- add an item to the estimator
CREATE FUNCTION probabilistic_add_item(counter probabilistic_estimator, item anyelement) RETURNS void
     AS 'MODULE_PATHNAME', 'probabilistic_add_item'
     LANGUAGE C;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION probabilistic_get_estimate(counter probabilistic_estimator) RETURNS real
     AS 'MODULE_PATHNAME', 'probabilistic_get_estimate'
     LANGUAGE C STRICT;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION probabilistic_reset(counter probabilistic_estimator) RETURNS void
     AS 'MODULE_PATHNAME', 'probabilistic_reset'
     LANGUAGE C STRICT;

-- length of the estimator (about the same as probabilistic_size with existing estimator)
CREATE FUNCTION length(counter probabilistic_estimator) RETURNS int
     AS 'MODULE_PATHNAME', 'probabilistic_length'
     LANGUAGE C STRICT;

/* functions for the aggregate functions */
CREATE FUNCTION probabilistic_add_item_agg(counter probabilistic_estimator, item anyelement, nbytes int, nsalts int) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_add_item_agg'
     LANGUAGE C;

CREATE FUNCTION probabilistic_add_item_agg2(counter probabilistic_estimator, item anyelement) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_add_item_agg2'
     LANGUAGE C;

/* input/output functions */
CREATE FUNCTION probabilistic_in(value cstring) RETURNS probabilistic_estimator
     AS 'MODULE_PATHNAME', 'probabilistic_in'
     LANGUAGE C STRICT;

CREATE FUNCTION probabilistic_out(counter probabilistic_estimator) RETURNS cstring
     AS 'MODULE_PATHNAME', 'probabilistic_out'
     LANGUAGE C STRICT;

-- data type for the probabilistic based distinct estimator
CREATE TYPE probabilistic_estimator (
    INPUT = probabilistic_in,
    OUTPUT = probabilistic_out,
    LIKE  = bytea
);

-- probabilistic counting based aggregate
-- items / number of bytes / number of salts
CREATE AGGREGATE probabilistic_distinct(item anyelement, nbytes int, nsalts int)
(
    sfunc = probabilistic_add_item_agg,
    stype = probabilistic_estimator,
    finalfunc = probabilistic_get_estimate
);

CREATE AGGREGATE probabilistic_distinct(item anyelement)
(
    sfunc = probabilistic_add_item_agg2,
    stype = probabilistic_estimator,
    finalfunc = probabilistic_get_estimate
);
