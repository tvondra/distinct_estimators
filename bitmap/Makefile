MODULE_big = bitmap_counter
OBJS = src/bitmap_counter.o src/bitmap.o

EXTENSION = bitmap_counter
DATA = sql/bitmap_counter--1.3.5.sql sql/bitmap_counter--1.2.0--1.3.4.sql sql/bitmap_counter--1.3.4--1.3.5.sql
MODULES = bitmap_counter

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: bitmap_counter.so

bitmap_counter.so: $(OBJS)

%.o : src/%.c
