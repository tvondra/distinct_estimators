SuperLogLog Estimator
=====================

This is an implementation of SuperLogLog algorithm as described in the
paper "LogLog Counting of Large Cardinalities," published by Flajolet
and Durand in 2003. Generally it is an improved LogLog estimator, with
truncation and restriction rules.

Contents of the extension
-------------------------
The extension provides the following elements

* superloglog_estimator data type (may be used for columns, in PL/pgSQL)

* functions to work with the superloglog_estimator data type

    * `superloglog_size(error_rate real)`
    * `superloglog_init(error_rate real)`

    * `superloglog_add_item(counter superloglog_estimator, item text)`
    * `superloglog_add_item(counter superloglog_estimator, item int)`

    * `superloglog_get_estimate(counter superloglog_estimator)`
    * `superloglog_reset(counter superloglog_estimator)`

    * `length(counter superloglog_estimator)`

  The purpose of the functions is quite obvious from the names,
  alternatively consult the SQL script for more details.

* aggregate functions 

    * `superloglog_distinct(text, real)`
    * `superloglog_distinct(text)`

    * `superloglog_distinct(int, real)`
    * `superloglog_distinct(int)`

  where the 1-parameter version uses default error rate 2.5%. That's
  quite generous and it may result in unnecessarily large estimators, so
  if you can work with worse error rate, pass the parameter explicitly.


Usage
-----
Using the aggregate is quite straightforward - just use it like a
regular aggregate function

    db=# SELECT superloglog_distinct(i, 0.01)
         FROM generate_series(1,100000) s(i);

and you can use it from a PL/pgSQL (or another PL) like this:

    DO LANGUAGE plpgsql $$
    DECLARE
        v_counter superloglog_estimator := superloglog_init(0.01);
        v_estimate real;
    BEGIN
        PERFORM superloglog_add_item(v_counter, 1);
        PERFORM superloglog_add_item(v_counter, 2);
        PERFORM superloglog_add_item(v_counter, 3);

        SELECT superloglog_get_estimate(v_counter) INTO v_estimate;

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
