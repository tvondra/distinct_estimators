\set ECHO 0
BEGIN;

-- disable the notices for the create script (shell types etc.)
SET client_min_messages = 'WARNING';
\i sql/bitmap_counter--1.3.2.sql
SET client_min_messages = 'NOTICE';

\set ECHO all


SELECT bitmap_distinct(id) BETWEEN 99000 AND 110000 val FROM generate_series(1,100000) s(id);

SELECT bitmap_distinct(id::text) BETWEEN 99000 AND 110000 val FROM generate_series(1,100000) s(id);

DO LANGUAGE plpgsql $$
DECLARE
    v_counter  bitmap_estimator := bitmap_init(0.01,10000);
    v_counter2 bitmap_estimator := bitmap_init(0.01,10000);
    v_estimate real;
    v_tmp real;
BEGIN

    FOR i IN 1..10000 LOOP
        PERFORM bitmap_add_item(v_counter, i);
        PERFORM bitmap_add_item(v_counter2, i::text);
    END LOOP;

    SELECT bitmap_get_estimate(v_counter) INTO v_estimate;
    IF (v_estimate BETWEEN 9900 AND 10100) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR';
    END IF;

    SELECT bitmap_get_error(v_counter) INTO v_tmp;
    RAISE NOTICE 'error = %',v_tmp;

    SELECT bitmap_get_ndistinct(v_counter) INTO v_tmp;
    RAISE NOTICE 'ndistinct = %',v_tmp;

    SELECT bitmap_get_estimate(v_counter2) INTO v_estimate;
    IF (v_estimate BETWEEN 9900 AND 10100) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR';
    END IF;

    SELECT bitmap_get_error(v_counter2) INTO v_tmp;
    RAISE NOTICE 'error = %',v_tmp;

    SELECT bitmap_get_ndistinct(v_counter2) INTO v_tmp;
    RAISE NOTICE 'ndistinct = %',v_tmp;

END$$;
ROLLBACK;
