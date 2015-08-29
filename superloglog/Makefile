MODULE_big = superloglog_counter
OBJS = src/superloglog_counter.o src/superloglog.o

EXTENSION = superloglog_counter
DATA = sql/superloglog_counter--1.2.2.sql sql/superloglog_counter--1.1.0--1.2.0.sql sql/superloglog_counter--1.2.0--1.2.1.sql sql/superloglog_counter--1.2.1--1.2.2.sql
MODULES = superloglog_counter

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: superloglog_counter.so

superloglog_counter.so: $(OBJS)

%.o : src/%.c
