HyperLogLog Estimator
=====================

This is an implementation of HyperLogLog algorithm as described in the
paper "HyperLogLog: the analysis of near-optimal cardinality estimation
algorithm", published by Flajolet, Fusy, Gandouet and Meunier in 2007.
Generally it is an improved version of LogLog algorithm with the last
step modified, to combine the parts using harmonic means.

This is not the only (or first) PostgreSQL extension implementing the
HyperLogLog estimator - since 2013/02 there's [postgresql-hll](https://github.com/aggregateknowledge/postgresql-hll)
It's a nice mature extension, so you may try it. I plan to write some
article comparing the pros/cons of the two implementations eventually.


Contents of the extension
-------------------------
The extension provides the following elements

* hyperloglog_estimator data type (may be used for columns, in PL/pgSQL)

* functions to work with the hyperloglog_estimator data type

    * `hyperloglog_size(error_rate real)`
    * `hyperloglog_init(error_rate real)`

    * `hyperloglog_add_item(counter hyperloglog_estimator, item anyelement)`

    * `hyperloglog_get_estimate(counter hyperloglog)`
    * `hyperloglog_reset(counter hyperloglog)`

    * `length(counter hyperloglog_estimator)`

  The purpose of the functions is quite obvious from the names,
  alternatively consult the SQL script for more details.

* aggregate functions 

    * `hyperloglog_distinct(anyelement, real)`
    * `hyperloglog_distinct(anyelement)`

  where the 1-parameter version uses default error rate 2%. That's
  quite generous and it may result in unnecessarily large estimators,
  so if you can work with lower precision, supply your error rate.


Usage
-----
Using the aggregate is quite straightforward - just use it like a
regular aggregate function

    db=# SELECT hyperloglog_distinct(i, 0.01)
         FROM generate_series(1,100000) s(i);

and you can use it from a PL/pgSQL (or another PL) like this:

    DO LANGUAGE plpgsql $$
    DECLARE
        v_counter hyperloglog_estimator := hyperloglog_init(32, 0.025);
        v_estimate real;
    BEGIN
        PERFORM hyperloglog_add_item(v_counter, 1);
        PERFORM hyperloglog_add_item(v_counter, 2);
        PERFORM hyperloglog_add_item(v_counter, 3);

        SELECT hyperloglog_get_estimate(v_counter) INTO v_estimate;

        RAISE NOTICE 'estimate = %',v_estimate;
    END$$;

And this can be done from a trigger (updating an estimate stored
in a table).


Problems
--------
Be careful about the implementation, as the estimators may easily
occupy several kilobytes (depends on the precision etc.). Keep in
mind that the PostgreSQL MVCC works so that it creates a copy of
the row on update, an that may easily lead to bloat. So group the
updates or something like that.

This is of course made worse by using unnecessarily large estimators,
so always tune the estimator to use the lowest amount of memory.
