MODULE_big = pcsa_counter
OBJS = pcsa_counter.o pcsa.o

EXTENSION = pcsa_counter
DATA = pcsa_counter--1.0.sql
MODULES = pcsa_counter

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: pcsa_counter.so

pcsa_counter.so: pcsa.o pcsa_counter.o

pcsa.o: pcsa.c

pcsa_counter.o: pcsa_counter.c
