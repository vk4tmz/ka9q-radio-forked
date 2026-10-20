CC ?= cc
CFLAGS ?= -O2 -g -std=gnu11 -Wall -Wextra -Werror
ROOT := ../..
BIN := $(ROOT)/build/ka9q-rx888-vhf-cli
all: $(BIN)
$(BIN): main.c cli_args.h
	@mkdir -p $(ROOT)/build
	$(CC) $(CFLAGS) main.c -lm -o $@
.PHONY: all

.PHONY: test
test: all
	$(CC) $(CFLAGS) -I$(ROOT)/src test_capture_sink.c -o $(ROOT)/build/test_rx888_capture_sink
	$(ROOT)/build/test_rx888_capture_sink
	$(CC) $(CFLAGS) test_cli.c -lm -o $(ROOT)/build/test_ka9q_rx888_cli
	$(ROOT)/build/test_ka9q_rx888_cli
