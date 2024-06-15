# Copyright 2022, 2023, 2024 Philip Kaludercic
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <https://www.gnu.org/licenses/>.

# Optional: CPPFLAGS = -DNDEBUG
CFLAGS   = -std=c11 -Wall -Wextra -Wformat=2 -Wuninitialized -Warray-bounds -Os -pipe
LDFLAGS  = -ldl

ifeq ($(shell basename $$(realpath $$(which $(CC)))),gcc)
ifeq (14,$(firstword $(sort $(shell $(CC) -dumpversion) 14)))
CFLAGS  := $(CFLAGS) -fhardened
LDFLAGS := $(LDFLAGS) -fhardened
else
CFLAGS  := $(CFLAGS) -fPIE -D_FORTIFY_SOURCE=2
LDFLAGS := $(LDFLAGS) -pie
endif
else
CFLAGS  := $(CFLAGS) -fPIE
LDFLAGS := $(LDFLAGS) -pie
endif

ifeq ($(shell id -u), 0)
PREFIX   = /usr/local
else
PREFIX   = $(HOME)/.local
endif

# The list of all functions that trip knows about are specified in
# ".db" files under the "db" directory.
DB      != find db -name '*.db'
GENSRC   = $(patsubst %.db,%.c,$(DB))
OBJ      = $(patsubst %.db,%.o,$(DB)) trip.o

AWK      = awk
RM       = rm -f

.PHONY: all
all: trip

trip: $(OBJ) fix-pie
	$(CC) -o $@ $(filter %.o, $^) $(LDFLAGS)
	./fix-pie $@

trip.o: $(GENSRC) Makefile
# See trip.c:/list of known commands/.  We collect and pipe all
# definitions into trip.c to generate a table of defined commands.
trip.o: CC := grep -h '^DEF' $(GENSRC) | sort -k2,2d -t, | $(CC)
$(OBJ): Makefile macs.h
macs.h: trip.h

$(GENSRC): %.c: %.db gen.awk Makefile
	$(AWK) -f gen.awk $< > $@

.PHONY: install
install: all
	install -Dpm 755 trip $(PREFIX)/bin
	install -Dpm 644 trip.1 $(PREFIX)/share/man/man1

.PHONY: uninstall
uninstall:
	$(RM) $(PREFIX)/bin/trip
	$(RM) $(PREFIX)/share/man/man1/trip.1

TAGS: trip.c trip.h macs.h
	etags $^

.PHONY: clean
clean:
	$(RM) $(GENSRC) $(OBJ) fix-pie trip TAGS
