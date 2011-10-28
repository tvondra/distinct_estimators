MODULE_big = bitmap_counter
OBJS = bitmap_counter.o bitmap.o

EXTENSION = bitmap_counter
DATA = bitmap_counter--1.0.sql
MODULES = bitmap_counter

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: bitmap_counter.so

bitmap_counter.so: bitmap.o bitmap_counter.o

bitmap.o: bitmap.c

bitmap_counter.o: bitmap_counter.c
