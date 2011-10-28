MODULE_big = probabilistic_counter
OBJS = probabilistic_counter.o probabilistic.o

EXTENSION = probabilistic_counter
DATA = probabilistic_counter--1.0.sql
MODULES = probabilistic_counter

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: probabilistic_counter.so

probabilistic_counter.so: probabilistic.o probabilistic_counter.o

probabilistic.o: probabilistic.c

probabilistic_counter.o: probabilistic_counter.c
