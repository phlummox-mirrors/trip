# Dummy Makefile to indicate that GNU Make should be used

.POSIX:
all trip:
	@if type gmake 2>&1 >/dev/null;			 \
	 then exec gmake --no-print-directory $@; 	 \
	 else echo "You need GNU Make to build trip" 1>&2; \
	 fi; false
