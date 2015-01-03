\set ECHO 0
BEGIN;

-- disable the notices for the create script (shell types etc.)
SET client_min_messages = 'WARNING';
\i sql/hyperloglog_counter--1.2.3.sql
SET client_min_messages = 'NOTICE';

\set ECHO all

SELECT hyperloglog_distinct(id, 0.02) BETWEEN 95000 AND 105000 val FROM generate_series(1,100000) s(id);

SELECT hyperloglog_distinct(id::text, 0.02) BETWEEN 95000 AND 105000 val FROM generate_series(1,100000) s(id);

DO LANGUAGE plpgsql $$
DECLARE
    v_counter  hyperloglog_estimator := hyperloglog_init(0.02);
    v_counter2 hyperloglog_estimator := hyperloglog_init(0.02);
    v_estimate real;
    v_tmp real;
BEGIN

    FOR i IN 1..100000 LOOP
        PERFORM hyperloglog_add_item(v_counter, i);
        PERFORM hyperloglog_add_item(v_counter2, i::text);
    END LOOP;

    SELECT hyperloglog_get_estimate(v_counter) INTO v_estimate;
    IF (v_estimate BETWEEN 90000 AND 110000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

    SELECT hyperloglog_get_estimate(v_counter2) INTO v_estimate;
    IF (v_estimate BETWEEN 90000 AND 110000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

END$$;
ROLLBACK;
