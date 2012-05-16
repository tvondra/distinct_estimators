MODULE_big = loglog_counter
OBJS = src/loglog_counter.o src/loglog.o

EXTENSION = loglog_counter
DATA = sql/loglog_counter--1.0.sql
MODULES = loglog_counter

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: loglog_counter.so

loglog_counter.so: $(OBJS)

%.o : src/%.c
