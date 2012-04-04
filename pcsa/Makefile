MODULE_big = pcsa_counter
OBJS = src/pcsa_counter.o src/pcsa.o

EXTENSION = pcsa_counter
DATA = sql/pcsa_counter--1.0.sql
MODULES = pcsa_counter

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: pcsa_counter.so

pcsa_counter.so: $(OBJS)

%.o : src/%.c
