# Standalone RX888 USB tool. Does not build radiod or require a running receiver.
CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra -Werror -std=gnu11
PKG_CFLAGS := $(shell pkg-config --cflags libusb-1.0)
PKG_LIBS := $(shell pkg-config --libs libusb-1.0)
ROOT := ../..
BIN := $(ROOT)/build/rx888-vhf-standalone
all: $(BIN)
$(BIN): rx888_vhf_standalone.c $(ROOT)/src/ezusb.c $(ROOT)/src/si5351.c $(ROOT)/src/si5351.h $(ROOT)/src/rational.h $(ROOT)/src/ezusb.h $(ROOT)/src/fx3_transfer_result.h $(ROOT)/src/config_paths.h rx888_vhf_calibration.h
	@mkdir -p $(ROOT)/build
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -I$(ROOT)/src rx888_vhf_standalone.c $(ROOT)/src/ezusb.c $(ROOT)/src/si5351.c -o $@ $(PKG_LIBS) -lm
$(ROOT)/src/config_paths.h:
	$(MAKE) -C $(ROOT)/src config_paths.h
.PHONY: all
