.DEFAULT_GOAL := all
VERSION ?= 1.0.0
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
DESTDIR ?=
TOOLS ?= prayer hijri
CC ?= cc
PYTHON ?= python3
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
HARDEN ?= 1
STATIC ?= 0
SYSTEM_LIBMUSLIM ?= 0
SYSTEM_CJSON ?= $(shell $(PKG_CONFIG) --exists libcjson 2>/dev/null && echo 1 || echo 0)

MUSLIM_SYSTEM_DIR := $(firstword $(foreach dir,/usr/local/include/libmuslim /usr/include/libmuslim /usr/local/include /usr/include,$(if $(wildcard $(dir)/prayertimes.h),$(dir))))
ifeq ($(SYSTEM_LIBMUSLIM),1)
MUSLIM_DIR ?= $(if $(MUSLIM_SYSTEM_DIR),$(MUSLIM_SYSTEM_DIR),/usr/include/libmuslim)
else
MUSLIM_DIR ?= $(if $(MUSLIM_SYSTEM_DIR),$(MUSLIM_SYSTEM_DIR),libmuslim)
endif

PROJECT_CPPFLAGS = -D_POSIX_C_SOURCE=200809L -DSUITE_VERSION='"$(VERSION)"' -Icommon -Iprayer/src -I$(MUSLIM_DIR)
PROJECT_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections
PROJECT_LDFLAGS = -Wl,--gc-sections
ifeq ($(HARDEN),1)
PROJECT_CFLAGS += -fstack-protector-strong
PROJECT_LDFLAGS += -Wl,-z,relro,-z,now
ifeq ($(STATIC),0)
PROJECT_CFLAGS += -fPIE
PROJECT_LDFLAGS += -pie
endif
endif
ifeq ($(STATIC),1)
PROJECT_LDFLAGS += -static
endif

ifeq ($(SYSTEM_CJSON),1)
PROJECT_CPPFLAGS += $(shell $(PKG_CONFIG) --cflags libcjson)
CJSON_LIBS = $(shell $(PKG_CONFIG) $(if $(filter 1,$(STATIC)),--static) --libs libcjson)
CJSON_OBJECTS =
else
PROJECT_CPPFLAGS += -Ivendor/cjson
CJSON_OBJECTS = build/vendor/cjson/cJSON.o
endif

COMMON_OBJECTS = build/common/util.o build/common/calendar.o
PRAYER_OBJECTS = build/prayer/src/prayer.o build/prayer/src/schedule.o \
                 build/common/location.o build/common/json.o $(COMMON_OBJECTS) $(CJSON_OBJECTS)
HIJRI_OBJECTS = build/hijri/src/hijri.o $(COMMON_OBJECTS)
BINARIES = $(foreach tool,$(TOOLS),$(tool)/$(tool))
TESTS = build/tests/test_core $(if $(filter prayer,$(TOOLS)),build/tests/test_library build/tests/test_schedule)

.PHONY: all check check-unit install uninstall clean dist FORCE
all: $(BINARIES)

FORCE:

build/config: FORCE
	@mkdir -p $(@D)
	@printf '%s\n' "$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(PROJECT_CFLAGS) $(LDFLAGS) $(PROJECT_LDFLAGS) $(LDLIBS) $(CJSON_LIBS) $(SYSTEM_CJSON)" > $@.tmp
	@cmp -s $@.tmp $@ || mv $@.tmp $@
	@rm -f $@.tmp

$(sort $(PRAYER_OBJECTS) $(HIJRI_OBJECTS)): build/config

prayer/prayer: $(PRAYER_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(PROJECT_LDFLAGS) $^ $(CJSON_LIBS) $(LDLIBS) -lm -o $@

hijri/hijri: $(HIJRI_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(PROJECT_LDFLAGS) $^ $(LDLIBS) -lm -o $@

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(PROJECT_CFLAGS) -MMD -MP -c $< -o $@

build/tests/test_core: tests/test_core.c $(COMMON_OBJECTS) build/config
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(PROJECT_CFLAGS) -UNDEBUG $(LDFLAGS) $(PROJECT_LDFLAGS) $(filter-out build/config,$^) $(LDLIBS) -lm -o $@

build/tests/test_library: tests/test_library.c $(MUSLIM_DIR)/prayertimes.h $(MUSLIM_DIR)/hijri.h build/config
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(PROJECT_CFLAGS) -UNDEBUG $(LDFLAGS) $(PROJECT_LDFLAGS) $< $(LDLIBS) -lm -o $@

build/tests/test_schedule: tests/test_schedule.c build/prayer/src/schedule.o build/common/util.o build/config
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(PROJECT_CFLAGS) -UNDEBUG $(LDFLAGS) $(PROJECT_LDFLAGS) $(filter-out build/config,$^) $(LDLIBS) -lm -o $@

check-unit: $(TESTS)
	@set -e; for test in $(TESTS); do ./$$test; done

check: all check-unit
	$(PYTHON) tests/test_cli.py $(TOOLS)

install: all
	install -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(MANDIR)/man1"
	@set -e; for tool in $(TOOLS); do \
		install -m 0755 $$tool/$$tool "$(DESTDIR)$(BINDIR)/$$tool"; \
		install -m 0644 man/$$tool.1 "$(DESTDIR)$(MANDIR)/man1/$$tool.1"; \
	done

uninstall:
	@set -e; for tool in $(TOOLS); do \
		rm -f "$(DESTDIR)$(BINDIR)/$$tool" "$(DESTDIR)$(MANDIR)/man1/$$tool.1"; \
	done

clean:
	rm -rf build
	rm -f prayer/prayer hijri/hijri

dist:
	@mkdir -p dist
	@set -e; for tool in $(TOOLS); do \
		tar -czf dist/$$tool-$(VERSION).tar.gz --transform="s,^,$$tool-$(VERSION)/," \
		--exclude=prayer/prayer --exclude=hijri/hijri \
		LICENSE Makefile README.md common prayer hijri libmuslim vendor tests man packaging examples; \
	done

-include $(wildcard build/common/*.d build/prayer/src/*.d build/hijri/src/*.d build/vendor/cjson/*.d)