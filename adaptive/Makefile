MODULE_big = adaptive_counter
OBJS = adaptive_counter.o adaptive.o

EXTENSION = adaptive_counter
DATA = adaptive_counter--1.0.sql
MODULES = adaptive_counter

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: adaptive_counter.so

adaptive_counter.so: adaptive.o adaptive_counter.o

adaptive.o: adaptive.c

adaptive_counter.o: adaptive_counter.c
