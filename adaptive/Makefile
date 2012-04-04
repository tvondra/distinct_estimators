MODULE_big = adaptive_counter
OBJS = src/adaptive_counter.o src/adaptive.o

EXTENSION = adaptive_counter
DATA = sql/adaptive_counter--1.0.sql
MODULES = adaptive_counter

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: adaptive_counter.so

adaptive_counter.so: $(OBJS)

%.o : src/%.c
