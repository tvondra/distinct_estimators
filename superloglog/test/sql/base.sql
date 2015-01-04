\set ECHO 0
BEGIN;

-- disable the notices for the create script (shell types etc.)
SET client_min_messages = 'WARNING';
\i sql/superloglog_counter--1.2.0.sql
SET client_min_messages = 'NOTICE';

\set ECHO all

SELECT superloglog_distinct(id, 0.02) BETWEEN 66000 AND 125000 val FROM generate_series(1,100000) s(id);

SELECT superloglog_distinct(id::text, 0.02) BETWEEN 66000 AND 125000 val FROM generate_series(1,100000) s(id);

DO LANGUAGE plpgsql $$
DECLARE
    v_counter  superloglog_estimator := superloglog_init(0.02);
    v_counter2 superloglog_estimator := superloglog_init(0.02);
    v_estimate real;
    v_tmp real;
BEGIN

    FOR i IN 1..100000 LOOP
        PERFORM superloglog_add_item(v_counter, i);
        PERFORM superloglog_add_item(v_counter2, i::text);
    END LOOP;

    SELECT superloglog_get_estimate(v_counter) INTO v_estimate;
    IF (v_estimate BETWEEN 66000 AND 125000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

    SELECT superloglog_get_estimate(v_counter2) INTO v_estimate;
    IF (v_estimate BETWEEN 66000 AND 125000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

END$$;
ROLLBACK;
