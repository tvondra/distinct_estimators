Probabilistic Estimator
=======================

This is an implementation of "probabilistic counter" as described in the
article "Probalistic Counting Algorithms for Data Base Applications",
published by Flajolet and Martin in 1985.


Contents of the extension
-------------------------
The extension provides the following elements

* probabilistic_estimator data type (may be used for columns, in PL/pgSQL)

* functions to work with the probabilistic_estimator data type

    * `probabilistic_size(nbytes int, nsalts int)`
    * `probabilistic_init(nbytes int, nsalts int)`

    * `probabilistic_add_item(counter probabilistic_estimator, item text)`
    * `probabilistic_add_item(counter probabilistic_estimator, item int)`

    * `probabilistic_get_estimate(counter probabilistic_estimator)`
    * `probabilistic_reset(counter probabilistic_estimator)`

    * `length(counter probabilistic_estimator)`

  The purpose of the functions is quite obvious from the names,
  alternatively consult the SQL script for more details.

* aggregate functions 

    * `probabilistic_distinct(text, int, int)`
    * `probabilistic_distinct(text)`

    * `probabilistic_distinct(int, int, int)`
    * `probabilistic_distinct(int)`

  where the 1-parameter version uses 4 bytes and 32 salts as
  default values for the two parameters. That's quite generous
  and it may result in unnecessarily large estimators, so if you
  can work with lower precision / expect less distinct values,
  pass the parameters explicitly.


Usage
-----
Using the aggregate is quite straightforward - just use it like a
regular aggregate function

    db=# SELECT probabilistic_distinct(i, 4, 32)
         FROM generate_series(1,100000) s(i);

and you can use it from a PL/pgSQL (or another PL) like this:

    DO LANGUAGE plpgsql $$
    DECLARE
        v_counter probabilistic_estimator := probabilistic_init(4, 32);
        v_estimate real;
    BEGIN
        PERFORM probabilistic_add_item(v_counter, 1);
        PERFORM probabilistic_add_item(v_counter, 2);
        PERFORM probabilistic_add_item(v_counter, 3);

        SELECT probabilistic_get_estimate(v_counter) INTO v_estimate;

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
