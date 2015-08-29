\set ECHO none
BEGIN;

-- disable the notices for the create script (shell types etc.)
SET client_min_messages = 'WARNING';
\i sql/loglog_counter--1.2.4.sql
SET client_min_messages = 'NOTICE';

\set ECHO all

SELECT loglog_distinct(id, 0.02) BETWEEN 90000 AND 110000 val FROM generate_series(1,100000) s(id);

SELECT loglog_distinct(id::text, 0.02) BETWEEN 90000 AND 110000 val FROM generate_series(1,100000) s(id);

DO LANGUAGE plpgsql $$
DECLARE
    v_counter  loglog_estimator := loglog_init(0.02);
    v_counter2 loglog_estimator := loglog_init(0.02);
    v_estimate real;
    v_tmp real;
BEGIN

    FOR i IN 1..100000 LOOP
        PERFORM loglog_add_item(v_counter, i);
        PERFORM loglog_add_item(v_counter2, i::text);
    END LOOP;

    SELECT loglog_get_estimate(v_counter) INTO v_estimate;
    IF (v_estimate BETWEEN 90000 AND 110000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

    SELECT loglog_get_estimate(v_counter2) INTO v_estimate;
    IF (v_estimate BETWEEN 90000 AND 110000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

END$$;
ROLLBACK;
