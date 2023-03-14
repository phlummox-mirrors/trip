# Copyright 2022, 2023 Philip Kaludercic
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
CFLAGS   = -std=gnu99 -fPIC -Wall -Wextra -Werror -ggdb3
LDFLAGS  = -ldl

ifeq ($(shell whoami), root)
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
trip.o: CC := grep -h '^DEF' $(GENSRC) | sort | $(CC)
$(OBJ): macs.h trip.h

$(GENSRC): %.c: %.d gen.awk Makefile
	$(AWK) -f gen.awk $< > $@

.PHONY: install
install: all
	echo install -Dpm 755 trip $(PREFIX)/bin
	echo install -Dpm 644 trip.1 $(PREFIX)/share/man/man/1

.PHONY: uninstall
uninstall:
	$(RM) $(PREFIX)/bin/trip
	$(RM) $(PREFIX)/share/man/man1/trip.1

.PHONY: clean
clean:
	$(RM) $(GENSRC) $(OBJ) fix-pie
