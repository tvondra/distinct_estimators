MODULE_big = adaptive_counter
OBJS = src/adaptive_counter.o src/adaptive.o

EXTENSION = adaptive_counter
DATA = sql/adaptive_counter--1.3.2.sql sql/adaptive_counter--1.2.0--1.3.0.sql sql/adaptive_counter--1.3.0--1.3.2.sql
MODULES = adaptive_counter

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: adaptive_counter.so

adaptive_counter.so: $(OBJS)

%.o : src/%.c
