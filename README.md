Trip
====

> _A false step; a stumble; a misstep; a loss of footing or balance.
> Fig.: An error; a failure; a mistake._  [1913 Webster]

Find here the source for "trip", a program that can intentionally
simulate failing C library function invocations.

	$ trip mkdir some-program-that-creates-a-directory
	mkdir: Too many links

The utility uses `LD_PRELOAD` to intercept libc function calls and
"trip" them by immediately returning an error code.  For more details,
please read the "trip" manual page (`trip.1` in this directory).

Installation
------------

On a GNU/Linux system, you should be able to build and install in a
single `make` call:

	$ make install

Note that by default this will install `trip` and the `trip` man page
under `~/.local`, so you should check if `~/.local/bin` is in your
`PATH`.  If you run the above command as the super user,

	$ make
	# make install

then the Makefile with install "trip" globally, targeting
`/usr/local`.  If neither of the two fit your preferred designation,
you can always override the `PREFIX` macro while invoking make:

	$ make -e PREFIX=/some/other/directory

Development
-----------

Trip is currently developed on [a GitLab instance], but I am fine with
accepting [patches] sent directly to [me] via Email.  Keep in mind that
the program is currently still experimental and does not always behave
the way it should.  If you experience any such difficulties, do not
hesitate to reach out so to solve the problem.

[a GitLab instance]:
	https://gitlab.cs.fau.de/oj14ozun/trip/
[patches]:
	https://git-send-email.io/
[me]:
	https://wwwcip.cs.fau.de/~oj14ozun/#contact
