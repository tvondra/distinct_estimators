MODULE_big = hyperloglog_counter
OBJS = src/hyperloglog_counter.o src/hyperloglog.o

EXTENSION = hyperloglog_counter
DATA = sql/hyperloglog_counter--1.1.0--1.2.0.sql  sql/hyperloglog_counter--1.2.0--1.2.3.sql  sql/hyperloglog_counter--1.2.3.sql
MODULES = hyperloglog_counter

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: hyperloglog_counter.so

hyperloglog_counter.so: $(OBJS)

%.o : src/%.c
