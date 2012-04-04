MODULE_big = bitmap_counter
OBJS = src/bitmap_counter.o src/bitmap.o

EXTENSION = bitmap_counter
DATA = sql/bitmap_counter--1.0.sql
MODULES = bitmap_counter

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: bitmap_counter.so

bitmap_counter.so: $(OBJS)

%.o : src/%.c
