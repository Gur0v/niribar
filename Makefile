CC ?= cc
SWAY ?= ../sway
BUILD := build
PREFIX ?= /usr/local

PKGS := wayland-client wayland-cursor cairo pangocairo json-c
CFLAGS += -std=c11 -D_POSIX_C_SOURCE=200809L -DWLR_USE_UNSTABLE \
	-Wall -Wextra -Wno-unused-parameter -Wno-unused-result \
	-Iinclude -I$(BUILD)/include -I$(SWAY)/include $(shell pkg-config --cflags $(PKGS))
LDLIBS += $(shell pkg-config --libs $(PKGS)) -lm

SOURCES := \
	src/main.c src/config.c src/ipc.c \
	$(SWAY)/swaybar/bar.c $(SWAY)/swaybar/config.c \
	$(SWAY)/swaybar/i3bar.c \
	$(SWAY)/swaybar/input.c $(SWAY)/swaybar/render.c \
	$(SWAY)/swaybar/status_line.c \
	$(SWAY)/client/pool-buffer.c \
	$(SWAY)/common/cairo.c $(SWAY)/common/ipc-client.c \
	$(SWAY)/common/log.c $(SWAY)/common/loop.c $(SWAY)/common/list.c \
	$(SWAY)/common/pango.c $(SWAY)/common/stringop.c $(SWAY)/common/util.c \
	$(BUILD)/protocols/wlr-layer-shell-unstable-v1.c \
	$(BUILD)/protocols/xdg-output-unstable-v1.c \
	$(BUILD)/protocols/cursor-shape-v1.c \
	$(BUILD)/protocols/xdg-shell.c \
	$(BUILD)/protocols/tablet-unstable-v2.c

.PHONY: all clean install test
all: niribar

install: niribar
	install -Dm755 niribar $(DESTDIR)$(PREFIX)/bin/niribar
	install -Dm644 niribar.1 $(DESTDIR)$(PREFIX)/share/man/man1/niribar.1

test: $(BUILD)/include/config.h $(BUILD)/include/wlr-layer-shell-unstable-v1-client-protocol.h
	$(CC) $(CFLAGS) tests/config.c src/config.c \
		$(SWAY)/swaybar/config.c $(SWAY)/common/list.c \
		$(SWAY)/common/log.c $(SWAY)/common/stringop.c $(SWAY)/common/util.c \
		$(LDLIBS) -o $(BUILD)/test-config
	$(BUILD)/test-config
	$(CC) $(CFLAGS) -ffunction-sections tests/ipc.c src/ipc.c \
		$(LDLIBS) -Wl,--gc-sections -o $(BUILD)/test-ipc
	$(BUILD)/test-ipc

niribar: $(SOURCES) $(BUILD)/include/config.h $(BUILD)/include/wlr-layer-shell-unstable-v1-client-protocol.h $(BUILD)/include/xdg-output-unstable-v1-client-protocol.h $(BUILD)/include/cursor-shape-v1-client-protocol.h
	$(CC) $(CFLAGS) $(SOURCES) $(LDLIBS) -o $@

$(BUILD)/include/config.h:
	@mkdir -p $(@D)
	@printf '%s\n' \
		'#define HAVE_GDK_PIXBUF 0' \
		'#define HAVE_TRAY 0' \
		'#define SWAY_VERSION "niribar"' > $@

define protocol
$(BUILD)/include/$(1)-client-protocol.h: $(2)
	@mkdir -p $$(@D)
	wayland-scanner client-header $$< $$@

$(BUILD)/protocols/$(1).c: $(2)
	@mkdir -p $$(@D)
	wayland-scanner private-code $$< $$@
endef

$(eval $(call protocol,wlr-layer-shell-unstable-v1,$(SWAY)/protocols/wlr-layer-shell-unstable-v1.xml))
$(eval $(call protocol,xdg-output-unstable-v1,$(shell pkg-config --variable=pkgdatadir wayland-protocols)/unstable/xdg-output/xdg-output-unstable-v1.xml))
$(eval $(call protocol,cursor-shape-v1,$(shell pkg-config --variable=pkgdatadir wayland-protocols)/staging/cursor-shape/cursor-shape-v1.xml))
$(eval $(call protocol,xdg-shell,$(shell pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml))
$(eval $(call protocol,tablet-unstable-v2,$(shell pkg-config --variable=pkgdatadir wayland-protocols)/unstable/tablet/tablet-unstable-v2.xml))

clean:
	rm -rf $(BUILD) niribar
