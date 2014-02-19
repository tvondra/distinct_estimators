LogLog Estimator
================

This is an implementation of LogLog algorithm as described in the paper
"LogLog Counting of Large Cardinalities," published by Flajolet and
Durand in 2003.

Contents of the extension
-------------------------
The extension provides the following elements

* loglog_estimator data type (may be used for columns, in PL/pgSQL)

* functions to work with the loglog_estimator data type

    * `loglog_size(error_rate real)`
    * `loglog_init(error_rate real)`

    * `loglog_add_item(counter loglog_estimator, item text)`
    * `loglog_add_item(counter loglog_estimator, item int)`

    * `loglog_get_estimate(counter loglog_estimator)`
    * `loglog_reset(counter loglog_estimator)`

    * `length(counter loglog_estimator)`

  The purpose of the functions is quite obvious from the names,
  alternatively consult the SQL script for more details.

* aggregate functions 

    * `loglog_distinct(text, real)`
    * `loglog_distinct(text)`

    * `loglog_distinct(int, real)`
    * `loglog_distinct(int)`

  where the 1-parameter version uses default error rate 2.5%. If you
  can work with lower precision, pass the parameter explicitly.


Usage
-----
Using the aggregate is quite straightforward - just use it like a
regular aggregate function

    db=# SELECT loglog_distinct(i, 0.01)
         FROM generate_series(1,100000) s(i);

and you can use it from a PL/pgSQL (or another PL) like this:

    DO LANGUAGE plpgsql $$
    DECLARE
        v_counter loglog_estimator := loglog_init(0.01);
        v_estimate real;
    BEGIN
        PERFORM loglog_add_item(v_counter, 1);
        PERFORM loglog_add_item(v_counter, 2);
        PERFORM loglog_add_item(v_counter, 3);

        SELECT loglog_get_estimate(v_counter) INTO v_estimate;

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
