MODULE_big = pcsa_counter
OBJS = src/pcsa_counter.o src/pcsa.o

EXTENSION = pcsa_counter
DATA = sql/pcsa_counter--1.3.0.sql  sql/pcsa_counter--1.2.0--1.3.0.sql
MODULES = pcsa_counter

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: pcsa_counter.so

pcsa_counter.so: $(OBJS)

%.o : src/%.c
