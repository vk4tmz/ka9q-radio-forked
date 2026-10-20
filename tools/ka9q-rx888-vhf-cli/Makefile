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

# Build and load the actual KA9Q plugin without claiming USB.
# Export the executable's KA9Q service symbols via -rdynamic.
PROBE := $(ROOT)/build/ka9q-rx888-driver-probe
KA9Q_OBJECTS := $(ROOT)/src/config.o $(ROOT)/src/parse.o $(ROOT)/src/timing.o $(ROOT)/src/misc.o $(ROOT)/src/sched.o
$(PROBE): driver_probe.c $(KA9Q_OBJECTS)
	$(CC) $(CFLAGS) -D_GNU_SOURCE=1 -iquote $(ROOT)/src $< $(KA9Q_OBJECTS) -rdynamic -o $@ -liniparser -ldl -lm -lpthread -lbsd -lrt
.PHONY: driver-probe
driver-probe:
	$(MAKE) -C $(ROOT)/src config.o parse.o timing.o misc.o sched.o
	$(MAKE) $(PROBE)
	$(PROBE) $(ROOT)/src/rx888.so
