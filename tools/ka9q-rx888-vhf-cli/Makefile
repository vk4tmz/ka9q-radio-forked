CC ?= cc
CFLAGS ?= -O2 -g -std=gnu11 -Wall -Wextra -Werror
ROOT := ../..
BIN := $(ROOT)/build/ka9q-rx888-vhf-cli
all: $(BIN)
$(BIN): main.c cli_args.h
	@mkdir -p $(ROOT)/build
	$(CC) $(CFLAGS) main.c -lm -o $@
.PHONY: all
