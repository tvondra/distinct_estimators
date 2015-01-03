MODULE_big = probabilistic_counter
OBJS = src/probabilistic_counter.o src/probabilistic.o

EXTENSION = probabilistic_counter
DATA = sql/probabilistic_counter--1.3.2.sql sql/probabilistic_counter--1.2.0--1.3.0.sql sql/probabilistic_counter--1.3.0--1.3.2.sql
MODULES = probabilistic_counter

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: probabilistic_counter.so

probabilistic_counter.so: $(OBJS)

%.o : src/%.c
