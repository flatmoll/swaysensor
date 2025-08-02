# The purpose of this Makefile is to build and syntax check the program.
#
# To customize C or linker flags, edit $(CFLAGS) or $(LDFLAGS),
# or invoke the compiler manually. Please do not edit $(ALLCFLAGS);
# they ensure the bare minimum required for successfull compilation.
#
# Use 'make clean' to remove both executable and build directory.
# Use 'make check' to run preprocessor with all warnings enabled.
# 
# This program is written in C23; please use GCC version 15 or above,
# or GCC versions 9 through 14 with an experimental '-std=c2x' flag.

SHELL		:= /bin/sh
CC		:= gcc

SRCDIR		:= ./src
BUILDDIR	:= ./build
TARGET		:= swaysensor

CFLAGS		:= -Wall -g -pg
ALLCFLAGS	:= $(shell pkg-config --cflags gio-2.0 glib-2.0)

LDFLAGS		:= 
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
