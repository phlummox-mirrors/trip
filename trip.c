/* Copyright 2020-2023 Philip Kaludercic
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.  This program is
 * distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details. You should have received a copy of the
 * GNU General Public License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#define _FORTIFY_SOURCE 2

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "trip.h"

#define ENVCONFNAME "____TRIP_CONFIGURATION"
#define VERSION "0.1.0"
#define USAGE "Usage: %s [func[:chance[:errno]]] command args\n"

#define GS "\035"           /* group separator */
#define RS "\036"           /* record separator */
#define DELIM ":/,"  /* delimiters in the skip configuration */

/* Parsed configuration */
static unsigned count = 0;
static struct entry {
    char *name;
    double chance;
    int rate;
    int error;
} entries[1 << 8];

/* random number generator data */
static unsigned long rng[56];

/* print debugging information to standard error */
static bool debug_mode = false;

/* list of known commands */
#define DEF(ret, name, params, args, fail, ...)	{ #name, { __VA_ARGS__ }},
static struct {
    char *name;
    int errs[256/sizeof(int)-sizeof(char*)]; /* adjust if necessary */
} names[] = {
#include "/dev/stdin"
};
#undef DEF

#ifdef NDEBUG
#define debug(...) (void) 0
#else


static void
_debug(char *words[], unsigned n)
{
    typedef ssize_t (wt)(int, const void *, size_t);
    static wt *real_write = NULL;
    char *prefix = "[trip]";

    if (NULL == real_write) {
        real_write = ((wt*) dlsym(RTLD_NEXT, "write"));
        assert(NULL != real_write);
    }

    real_write(STDERR_FILENO, prefix, strlen(prefix));

    for (unsigned i = 0; i < n; ++i) {
         real_write(STDERR_FILENO, " ", 1);
         real_write(STDERR_FILENO, words[i], strlen(words[i]));
    }
    real_write(STDERR_FILENO, "\n", 1);
}

#define debug(...)                              \
    do {                                        \
        if (!debug_mode) break;                 \
        char *words[] = { __VA_ARGS__ };        \
        _debug(words, LENGTH(words));           \
    } while (0)

#endif

/* Check if a function is supported by trip */
static char*
check(const char *fn)
{
    for (unsigned i = 0; i < LENGTH(names); ++i)
         if (!strcmp(names[i].name, fn))
              return names[i].name;
    return NULL;
}

/* Function to parse the configuration */
static void
init()
{
    static atomic_flag done = ATOMIC_FLAG_INIT;
    static volatile bool ready = false;
    if (true == atomic_flag_test_and_set(&done)) {
        while (!ready);
        return;			/* prevent concurrent re-initialisation */
    }

    char *conf = getenv(ENVCONFNAME);
    if (NULL == conf) {		/* no configuration, no cry */
        ready = !ready;
        return;
    }

    if (conf[0] == 'D') {
        debug_mode = true;
        conf++;
        debug("debug mode enabled:", conf);
    }

    char copy[strlen(conf) + 1];
    strcpy(copy, conf);

    /* Parse environmental variable containing the configuration. */
    char *tok = NULL, *s1 = NULL, *s2 = NULL;
    while (NULL != (tok = strtok_r(tok ? NULL : copy, RS, &s1))) {
        if (count >= LENGTH(entries)) {
            dprintf(STDERR_FILENO, "Overlong configuration (%d >= %u)\n",
                    count, LENGTH(entries));
            abort();
        }

        struct entry *e = &entries[count];
        *e = (struct entry) { .name = strtok_r(tok, GS, &s2) };
        assert(NULL != e->name);	/* otherwise we wouldn't be here */

	char *dup;
	if ((dup = check(e->name)) == NULL) {
	  debug("unknown function", e->name);
	  continue;
	}
        debug("registering", e->name);
	e->name = dup;

        char *chance = strtok_r(NULL, GS, &s2);
        if (NULL == chance) {
            dprintf(STDERR_FILENO, "Malformed trip configuration \"%s\"\n", conf);
            abort();
        }

        char *end;
        errno = 0;
        e->chance = strtod(chance, &end);
        if ('\0' != *end || (0 == e->error && 0 != errno)) {
            dprintf(STDERR_FILENO, "Malformed trip chance \"%s\"", chance);
            if (errno != 0)
                 perror("strtod");
            abort();
        }

        char *code = strtok_r(NULL, GS, &s2);
        if (NULL == code) {
            dprintf(STDERR_FILENO, "Malformed trip code \"%s\"\n", conf);

            abort();
        }

        errno = 0;
        e->error = strtol(code, &end, 16);
        if (*end != '\0' || errno != 0) {
            dprintf(STDERR_FILENO, "Malformed trip error code \"%s\"\n", code);
            abort();
        }

        count++;
    }
    assert(count > 0);

    /* Initialise local PRNG (Mitchell-Moore, see TAOCP p. 26).  We use a
     * custom one so as to not interfere with rand from the standard
     * library. */
    unsigned i = 0;
    pid_t p = getpid();
    time_t t = time(NULL);
    rng[i] = p / t + t / p + t + p + t * p;
    for (i = 1; i < LENGTH(rng); ++i) {
        rng[i] = rng[i - 1] * p + t;
    }

    debug("initialised");
    ready = !ready;             /* spin-un-lock */
}

static unsigned long
next()                          /* ... "random" number */
{
    static size_t i = 0;
    unsigned long r =
        rng[(i - 24) % LENGTH(rng)] + rng[(i - 55) % LENGTH(rng)];
    return rng[i++ % LENGTH(rng)] = r;
}

static double
chance()
{
    return ((double) next()) / ((double) ULONG_MAX);
}

/* Failure predicate called by the trip stubs. */
bool
____trip_should_fail(char *name, int errv[], size_t errn)
{
    unsigned i;

    /* Initialise failure data if necessary */
    init();

    /* FIXME: Replace the associative array with something that has less
     * of an overhead. */
    debug("intercepting", name);
    for (i = 0; i < count; ++i) {
        if (!!strcmp(name, entries[i].name)) {
            continue;
        }

        debug("probing", name);
        if (chance() > entries[i].chance) {
            /* FIXME: If we have multiple entries on the same function,
             * their chances should be properly aggregated.  Currently, if
             * the first entry has a chance of P and the second one has a
             * chance of Q, the actual chance of the second entry will
             * turn out to be (1-P) * Q. */
            continue;
        }

        /* Update errno with either a random or the requested error *
         * value. */
        errno = entries[i].error == 0 && errn > 0 ? errv[next() % errn]
            : entries[i].error;

        debug("tripping", name);
        return true;
    }

    debug("forgiving", name);

    return false;
}

/* Resolve an errno NAME into a errno number.
 *
 * TODO: Avoid calling cpp more often than necessary by caching the
 * results. */
static int
derrno(char *name)
{
    char cmd[100];
    /* From popen(3): "Since a pipe is by definition unidirectional,
     * the type argument may specify only reading or writing, not
     * both; the re- sulting stream is correspondingly read-only or
     * write-only".
     *
     * For this reason we prepare a command that will pope the symbol
     * we want to resolve when invoking the command, so that we can
     * read the macro-expansion. */
    snprintf(cmd, sizeof(cmd), "echo \"%s\" | cpp -include errno.h", name);
    FILE *cpp = popen(cmd, "r");
    if (NULL == cpp) {
        perror("popen");
        return 0;
    }

    char line[1024];
    int val = -1;
    while ((fgets(line, sizeof(line), cpp)) != NULL) {
        if (sizeof(line) - 1 == strlen(line)
            && line[sizeof(line) - 2] != '\n') {
            int c;		/* empty out the rest of the line */
            while ((c = fgetc(cpp)), !(c == '\n' || c == EOF));
            if (c == EOF)
                 goto close;
        }

        if ('#' == line[0] || '\n' == line[0] || isalpha(line[0])) {
            continue;		/* ignore comments, empty lines and C
                                 * code (probably declarations). */
        }

        char *end;
        errno = 0;
        val = strtol(line, &end, 10);
        if (!('\0' == *end || '\n' == *end) || 0 != errno) {
            dprintf(STDERR_FILENO, "Expected a number \"%s\" (for %s)\n", line,
                    name);
            exit(EXIT_FAILURE);
        }

        break;
    }
    if (ferror(cpp)) {
        perror("fgets");
        exit(EXIT_FAILURE);
    }

  close:
    if (-1 == pclose(cpp)) {
        perror("popen");
        exit(EXIT_FAILURE);
    }

    if (0 > val) {
        dprintf(STDERR_FILENO, "Failed to resolve %s\n", line);
        exit(EXIT_FAILURE);
    }

    return val;
}

/* Parse and add an ENTRY to the table entries. */
static void
enter(char *entry)
{
    char *func, *chance, *error;
    func = strtok(entry, DELIM);
    if (check(func) == NULL) {
        dprintf(STDERR_FILENO, "Unknown function \"%s\", cannot trip\n", func);
        exit(EXIT_FAILURE);
    }

    chance = strtok(NULL, DELIM);
    if (NULL == chance) {
        error = NULL;
        goto skip;
    }
    error = strtok(NULL, DELIM);

    if (NULL == error && NULL != chance && (tolower(chance[0]) == 'e')) {
        error = chance;
        chance = NULL;
    }

  skip:
    entries[count] = (struct entry) { .name = func };

    char *end;
    errno = 0;
    double num = 1;
    if (NULL != chance) {
        num = strtod(chance, &end);
        if (*end != '\0' || (num == 0 && errno != 0)) {
            dprintf(STDERR_FILENO, "Cannot parse chance \"%s\"\n", chance);
            exit(EXIT_FAILURE);
        }
    }

    if (0 >= num) {
        dprintf(STDERR_FILENO, "The chance %s (for %s) is not positive\n", chance,
                func);
        exit(EXIT_FAILURE);
    }
    if (1 >= num) {
        entries[count].chance = num;
    } else {
        dprintf(STDERR_FILENO, "The chance %g (for %s) is greater than 1\n", num,
                func);
        exit(EXIT_FAILURE);
    }

    if (NULL != error) {
        for (unsigned i = 0; i < strlen(error); ++i) {
            error[i] = toupper(error[i]);
        }
        entries[count].error = derrno(error);
        for (unsigned i = 0; i < LENGTH(names); ++i) {
            if (!strcmp(names[i].name, func)) {
                for (unsigned j = 0; j < LENGTH(names[i].errs); ++j) {
                    if (names[i].errs[j] == entries[count].error) {
                        goto found_it;
                    }
                }
                break;
            }
        }
        dprintf(STDERR_FILENO, "%s is not expected to return %s\n", func, error);
        exit(EXIT_FAILURE);

      found_it:
        ;
    }

    count++;
}

/* List all supported functions */
static void __attribute__((noreturn))
list()
{
    for (unsigned i = 0; i < LENGTH(names); ++i) {
        puts(names[i].name);
    }
    exit(EXIT_SUCCESS);
}

/* Print a usage string and terminate */
static void __attribute__((noreturn))
usage(char *argv0)
{
    dprintf(STDERR_FILENO, USAGE, argv0);
    dprintf(STDERR_FILENO, "Flags:\n"
            "\t-m\tSpecify multiple functions to trip\n"
            "\t-l\tList all supported functions\n"
#ifndef NDEBUG
            "\t-d\tPrint debugging information\n"
#endif
            "\t-V\tPrint version\n"
            "\t-h\tPrint this message\n");
    exit(EXIT_SUCCESS);
}

int __attribute__((weak))
main(int argc, char *argv[])
{
    /* If the environmental variable is set, we are currently being
     * invoked instead of the actual main function.  As this is not
     * intended, we will call the right function instead. */
    char *conf = getenv(ENVCONFNAME);
    if (conf) {
        dprintf(STDERR_FILENO, "Don't trip me\n");
        exit(EXIT_FAILURE);
    }

    /* Otherwise we are being invoked to wrap an actual call.  Let us *
     * start by parsing the command line. */
    int opt;
    bool multi = false;
    while ((opt = getopt(argc, argv, "mlVdh")) != -1) {
        switch (opt) {
        case 'm':
            multi = true;
            break;
        case 'l':
            list();
        case 'V':
            dprintf(STDERR_FILENO, "trip (Version %s)\n", VERSION);
            exit(EXIT_SUCCESS);
#ifndef NDEBUG
        case 'd':
            debug_mode = true;
            break;
#endif
        case 'h':
        default:		/* '?' */
            usage(argv[0]);
        }
    }

    if (optind >= argc) {
        usage(argv[0]);
    }

    if (multi) {
        while (optind < argc && !!strcmp(argv[optind], ".")) {
            enter(argv[optind]);
            optind++;
        }
	if (argc <= optind) {
            dprintf(STDERR_FILENO, "Unterminated list of functions\n");
            exit(EXIT_FAILURE);
	}
        optind++;
    } else {
        enter(argv[optind]);
        optind++;
    }

    if (optind >= argc) {
        usage(argv[0]);
    }

    /* Construct configuration */
    size_t size = strlen(ENVCONFNAME) + 1 + 1;
    conf = malloc(size + 1);
    if (NULL == conf) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(conf, ENVCONFNAME "=");
    if (debug_mode) {
         strcat(conf, "D");
         /* we have allocated a byte more than indicated in size, for
          * this debugging indicator */
         size++;
    }

    for (unsigned i = 0; i < count; ++i) {
        for (size_t n = 1024;; n <<= 1) {
            char ent[n];
            if (n == (size_t) snprintf(ent, sizeof(ent),
                                       "%s" GS "%a" GS "%x" RS,
                                       entries[i].name, entries[i].chance,
                                       entries[i].error)) {
                continue;	/* we need more memory */
            }

            size += 1 + strlen(ent) + 1;
            conf = realloc(conf, size);
            if (!conf) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }

            strcat(conf, ent);

            break;
        }
    }

    /* Get path to the shared library */
    char preload[1 << 12];

#ifdef __linux__
    strcpy(preload, "LD_PRELOAD=");
    unsigned prefix = strlen(preload);
    int ret = readlink("/proc/self/exe", preload + prefix,
                       sizeof(preload) - prefix);
    if (1 == -ret) {
        perror("readlink");
        exit(EXIT_FAILURE);
    }
    preload[prefix + ret] = '\0';
#else
#error "System is not supported"
#endif

    if (execvpe(argv[optind], argv + optind, (char*[]) {preload, conf, NULL})) {
        perror("exec");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}
