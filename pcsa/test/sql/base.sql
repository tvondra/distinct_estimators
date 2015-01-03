\set ECHO 0
BEGIN;

-- disable the notices for the create script (shell types etc.)
SET client_min_messages = 'WARNING';
\i sql/pcsa_counter--1.3.2.sql
SET client_min_messages = 'NOTICE';

\set ECHO all

SELECT pcsa_distinct(id, 32, 4) BETWEEN 90000 AND 110000 val FROM generate_series(1,100000) s(id);

SELECT pcsa_distinct(id::text, 32, 4) BETWEEN 90000 AND 110000 val FROM generate_series(1,100000) s(id);

DO LANGUAGE plpgsql $$
DECLARE
    v_counter  pcsa_estimator := pcsa_init(32, 4);
    v_counter2 pcsa_estimator := pcsa_init(32, 4);
    v_estimate real;
    v_tmp real;
BEGIN

    FOR i IN 1..10000 LOOP
        PERFORM pcsa_add_item(v_counter, i);
        PERFORM pcsa_add_item(v_counter2, i::text);
    END LOOP;

    SELECT pcsa_get_estimate(v_counter) INTO v_estimate;
    IF (v_estimate BETWEEN 8000 AND 11000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

    SELECT pcsa_get_estimate(v_counter2) INTO v_estimate;
    IF (v_estimate BETWEEN 8000 AND 11000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

END$$;

ROLLBACK;
