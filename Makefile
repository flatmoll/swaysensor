# SPDX-License-Identifier: MIT
# Makefile: assistance with building, syntax checking, and profiling.
# Copyright (C) 2025 Fuad Veliev <fuad@grrlz.net>
#
# To customize C or linker flags, edit $(CFLAGS) or $(LDFLAGS),
# respectively. Please do not edit $(ALLCFLAGS) or $(LDLIBS) -
# they ensure the minimum required for a successful compilation.
#
# Use 'make clean' to remove both executable and build directory.
# Use 'make check' to run preprocessor with all warnings enabled.

SHELL		:= /bin/sh
CC		:= gcc

SRCDIR		:= ./src
BUILDDIR	:= ./build
TARGET		:= swaysensor

CFLAGS		:= -Wall -g -pg
ALLCFLAGS	:= $(shell pkg-config --cflags gio-2.0 glib-2.0)

LDFLAGS		:= -pg
LDLIBS		:= $(shell pkg-config --libs gio-2.0 glib-2.0)

SOURCES		:= $(wildcard $(SRCDIR)/*.c)
OBJECTS		:= $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(ALLCFLAGS) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

.PHONY: all clean check

all: $(TARGET)

clean:
	rm -r $(BUILDDIR) $(TARGET)

check:
	$(CC) -Wall -Wextra -pedantic -fsyntax-only $(SOURCES)
