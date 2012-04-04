BEGIN;

CREATE EXTENSION adaptive_counter;

SELECT adaptive_distinct(id) BETWEEN 99000 AND 110000 val FROM generate_series(1,100000) s(id);

SELECT adaptive_distinct(id::text) BETWEEN 99000 AND 110000 val FROM generate_series(1,100000) s(id);

DO LANGUAGE plpgsql $$
DECLARE
    v_counter  adaptive_estimator := adaptive_init(0.01,10000);
    v_counter2 adaptive_estimator := adaptive_init(0.01,10000);
    v_estimate real;
    v_tmp real;
BEGIN

    FOR i IN 1..10000 LOOP
        PERFORM adaptive_add_item(v_counter, i);
        PERFORM adaptive_add_item(v_counter2, 10000 + i);
    END LOOP;

    SELECT adaptive_get_estimate(v_counter) INTO v_estimate;

    SELECT adaptive_get_error(v_counter) INTO v_tmp;
    RAISE NOTICE 'error = %',v_tmp;

    SELECT adaptive_get_ndistinct(v_counter) INTO v_tmp;
    RAISE NOTICE 'ndistinct = %',v_tmp;

    SELECT adaptive_get_item_size(v_counter) INTO v_tmp;
    RAISE NOTICE 'item size = %',v_tmp;

    IF (v_estimate BETWEEN 9900 AND 10100) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR';
    END IF;

    v_counter := adaptive_merge(v_counter, v_counter2);

    SELECT adaptive_get_estimate(v_counter) INTO v_estimate;

    SELECT adaptive_get_error(v_counter) INTO v_tmp;
    RAISE NOTICE 'error = %',v_tmp;

    SELECT adaptive_get_ndistinct(v_counter) INTO v_tmp;
    RAISE NOTICE 'ndistinct = %',v_tmp;

    SELECT adaptive_get_item_size(v_counter) INTO v_tmp;
    RAISE NOTICE 'item size = %',v_tmp;

    IF (v_estimate BETWEEN 2*9900 AND 2*10100) THEN
        RAISE NOTICE 'estimate OK';
    ELSE
        RAISE NOTICE 'estimate ERROR';
    END IF;

END$$;

ROLLBACK;
