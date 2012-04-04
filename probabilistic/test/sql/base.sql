BEGIN;

CREATE EXTENSION probabilistic_counter;

SELECT probabilistic_distinct(id, 4, 32) BETWEEN 90000 AND 110000 val FROM generate_series(1,100000) s(id);

SELECT probabilistic_distinct(id::text, 4, 32) BETWEEN 90000 AND 110000 val FROM generate_series(1,100000) s(id);

DO LANGUAGE plpgsql $$
DECLARE
    v_counter  probabilistic_estimator := probabilistic_init(4, 32);
    v_counter2 probabilistic_estimator := probabilistic_init(4, 32);
    v_estimate real;
    v_tmp real;
BEGIN

    FOR i IN 1..10000 LOOP
        PERFORM probabilistic_add_item(v_counter, i);
        PERFORM probabilistic_add_item(v_counter2, i::text);
    END LOOP;

    SELECT probabilistic_get_estimate(v_counter) INTO v_estimate;
    IF (v_estimate BETWEEN 8000 AND 11000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

    SELECT probabilistic_get_estimate(v_counter2) INTO v_estimate;
    IF (v_estimate BETWEEN 8000 AND 11000) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR (%)',v_estimate;
    END IF;

END$$;

ROLLBACK;
