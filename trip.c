/* Copyright 2020-2024 Philip Kaludercic
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

#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <assert.h>
#undef assert

#include "trip.h"

#define ENVCONFNAME "____TRIP_CONFIGURATION"
#define VERSION "0.1.0"
#define USAGE "Usage: %s [func[:chance[:errno]]] command args\n"

#ifndef COMPILER
#define COMPILER "unknown"
#endif

#define GS "\035"           /* group separator */
#define RS "\036"           /* record separator */
#define DELIM ":/"         /* delimiters in the skip configuration */

#define GLUE(A, B) A ## B
#define XGLUE(A, B) GLUE(A, B)

/* Parsed configuration */
static unsigned count = 0;
static struct entry {
    char *name;
    double chance;
    int rate;
    int error;
} entries[1 << 8];

/* are we currently operating as a dynamic library? */
static bool is_lib = true;

/* random number generator data */
static unsigned long rng[56];

/* print debugging information to standard error */
static bool debug_mode = false;

/* list of known commands */
#define DEF(ret, name, params, args, fail, ...)	{ #name, { __VA_ARGS__ }},
#define E(e) { .no = e, .name = #e }
static struct entry_name {
    char *name;
    struct {
        int no;
        char *name;
    } errs[256/sizeof(int)-sizeof(char*)]; /* adjust if necessary */
} names[] = {
#include "/dev/stdin"
};
#undef E
#undef DEF

static char *argv0;

static void __attribute__((noreturn))
fail(const char reason[static 1], const bool print_emsg)
{
    dprintf(STDERR_FILENO, "%s: %s%s%s\n",
            argv0,
            reason,
            print_emsg ? ": ": "",
            print_emsg ? strerror(errno) : "");
    exit(EXIT_FAILURE);
}

/* Declare and format a string on the stack. */
#define _$sprintf(buf, c, fmt, ...)                             \
    for (char buf[snprintf(NULL, 0, fmt, __VA_ARGS__) + 1],     \
             c = !'\0';                                         \
         c != '\0' &&                                           \
             (snprintf(buf, sizeof(buf), fmt, __VA_ARGS__), 1); \
         c = '\0')
#define $sprintf(buf, ...)                                      \
    _$sprintf(buf, XGLUE(__c_, __COUNTER__), __VA_ARGS__)

#define failf(fmt, ...)                                 \
    do $sprintf(_, fmt, __VA_ARGS__) fail(_, false);    \
    while (0)

#ifdef NDEBUG
#define assert(_)  do { (void) 0; } while (0)
#define debug(...) (void) 0
#define debugf(...) (void) 0
#else

static void _debug(char *words[], unsigned n);

static void
do_assert(char *val, unsigned linenr)
{
    $sprintf(line, "(%s:%d)", __FILE__, linenr) {
        _debug((char*[]){"Assertion failed", val, line}, 3);
    }
    abort();
}

#define assert(val)                             \
    do {                                        \
        if (!(val)) {                           \
            do_assert(#val, __LINE__);          \
        }                                       \
    } while (0)

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

#define debugf(fmt, ...)                        \
    do $sprintf(_, fmt, __VA_ARGS__) debug(_);  \
    while (0)

#endif

/* Helper function for qsort and bsearch */
static int
compar_name(const void *a, const void *b)
{
    return strcmp((((struct entry_name *) a))->name,
                  (((struct entry_name *) b))->name);
}

/* Check if a function is supported by trip */
static char* __attribute__((pure))
check(const char *fn)
{
    struct entry_name *entry =
        bsearch(&((struct entry_name) { .name = (char * const) fn }),
                names,
                LENGTH(names),
                sizeof(struct entry_name),
                compar_name);
    return entry != NULL ? entry->name : NULL;
}

static unsigned long
next(void)                          /* ... "random" number */
{
    assert(is_lib);
    static size_t i = 0;
    unsigned long r =
        rng[(i - 24) % LENGTH(rng)] + rng[(i - 55) % LENGTH(rng)];
    return rng[i++ % LENGTH(rng)] = r;
}

static double
chance(void)
{
    assert(is_lib);
    return ((double) next()) / ((double) ULONG_MAX);
}

/* Function to parse the configuration */
static void
init(void)
{
    assert (is_lib);

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
            failf("Overlong configuration (%d >= %u)", count, LENGTH(entries));
        }

        struct entry *e = &entries[count];
        *e = (struct entry) { .name = strtok_r(tok, GS, &s2) };
        assert(NULL != e->name); /* otherwise we wouldn't be here */

        char *dup;
        if ((dup = check(e->name)) == NULL) {
            debug("unknown function", e->name);
            continue;
        }
        debug("registering", e->name);
        e->name = dup;

        char *chance = strtok_r(NULL, GS, &s2);
        if (NULL == chance) {
            failf("Malformed trip configuration \"%s\"", conf);
        }

        char *end;
        errno = 0;
        e->chance = strtod(chance, &end);
        if ('\0' != *end || (0 == e->error && 0 != errno)) {
            $sprintf(_, "Malformed trip chance \"%s\"", chance) {
                fail(_, errno != 0);
            }
        }

        char *code = strtok_r(NULL, GS, &s2);
        if (NULL == code) {
            failf("Malformed trip code \"%s\"", conf);
        }

        errno = 0;
        e->error = (int) strtol(code, &end, 16);
        if (*end != '\0' || errno != 0) {
            failf("Malformed trip error code \"%s\"", code);
        }

        count++;
    }
    assert(count > 0);

    /* Initialise local PRNG (Mitchell-Moore, see TAOCP p. 26).  We use a
     * custom one so as to not interfere with rand from the standard
     * library. */
    unsigned i = 0;
    unsigned long p = (unsigned long) (getpid() + getppid());
    unsigned long t;
    struct timeval tv;
    if (0 == gettimeofday(&tv, NULL)) {
        t = (unsigned long) (tv.tv_usec + tv.tv_sec);
    } else {
        t = 1;
    }
    rng[i] = p / t + t / p + t + p + t * p;
    for (i = 1; i < LENGTH(rng); ++i) {
        rng[i] = rng[i - 1] * p + t;
    }

    unsigned long n = next(); (void) n;
    debugf("initial PRNG state is %lu", n);

    debug("initialised");
    ready = !ready;             /* spin-un-lock */
}

/* Failure predicate called by the trip stubs. */
bool
____trip_should_fail(char *name, int errv[], size_t errn)
{
    unsigned i;
    if (!is_lib) return false;

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

/* Parse and add an ENTRY to the table entries. */
static void
enter(char *entry)
{
    assert(!is_lib);

    char *func, *chance, *error;
    func = strtok(entry, DELIM);
    if (!func) {
        fail("Must pass a non-empty function name\n", false);
    }
    if (check(func) == NULL) {
        failf("Unknown function \"%s\", cannot trip", func);
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
            failf("Cannot parse chance \"%s\"", chance);
        }
    }

    if (0 >= num) {
        failf("The chance %s (for %s) is not positive", chance,
              func);
    }
    if (1 >= num) {
        entries[count].chance = num;
    } else {
        failf("The chance %g (for %s) is greater than 1", num,
              func);
    }

    if (NULL != error) {
        for (unsigned i = 0; i < strlen(error); ++i) {
            error[i] = (char) toupper((unsigned char) error[i]);
        }
        for (unsigned i = 0; i < LENGTH(names); ++i) {
            if (!strcmp(names[i].name, func)) {
                for (unsigned j = 0; j < LENGTH(names[i].errs); ++j) {
                    if (!strcmp(names[i].errs[j].name, error)) {
                        entries[count].error = names[i].errs[j].no;
                        goto found_it;
                    }
                }
                break;
            }
        }
        failf("%s is not expected to return %s", func, error);

      found_it:
        ;
    }

    count++;
}

/* List all supported functions */
static void __attribute__((noreturn))
list(const char *unused)
{
    (void) unused;
    assert(!is_lib);

    for (unsigned i = 0; i < LENGTH(names); ++i) {
        puts(names[i].name);
    }
    exit(EXIT_SUCCESS);
}

/* List all supported errno values for func. */
static void __attribute__((noreturn))
list_errors(const char *func)
{
    if (check(func) == NULL) {
        failf("Unknown function \"%s\"", func);
    }

    for (unsigned i = 0; i < LENGTH(names); ++i) {
        if (!strcmp(names[i].name, func)) {
            for (unsigned j = 0; names[i].errs[j].no != 0; ++j) {
                puts(names[i].errs[j].name);
            }
            exit(EXIT_SUCCESS);
        }
    }

    abort();
}

/* print a list of all functions that trip could affect */
static void __attribute__((noreturn))
check_exec(const char *exec)
{
    FILE *nm;
    char line[BUFSIZ], *PATH, *dir = NULL, *func;
    int status, fd;

    PATH = getenv("PATH");
    if (NULL == PATH) {
        fail("no $PATH set", false);
    }

    $sprintf(path_cpy, ".:%s", PATH) {
        nm = NULL;
        while ((dir = strtok(dir == NULL ? path_cpy : NULL, ":"))) {
            debugf("looking for %s in '%s'...", exec, dir);
            fd = open(dir, O_DIRECTORY);
            if (-1 == fd) { continue; } /* invalid component */

            if (0 == faccessat(fd, exec, X_OK, AT_EACCESS)) {
                debugf("found %s in '%s'", exec, dir);
                close(fd);

                assert(NULL != dir);
                $sprintf(cmd, "nm -uP '%s/%s'", dir, exec) {
                    debugf("executing \"%s\"", cmd);
                    nm = popen(cmd, "r");
                }
                if (NULL == nm) {
                    fail("popen", true);
                }
                break;
            }
            close(fd);
        }
        if (dir == NULL) {
            failf("failed to locate %s in $PATH", exec);
        }
    }
    assert(nm != NULL);

    while (NULL != fgets(line, sizeof line, nm)) {
        func = strtok(line, " @");
        if (func == NULL || check(func) == NULL) {
            continue;
        }

        puts(func);
    }
    if (ferror(nm)) {
        fail("fgets", true);
    }

    status = pclose(nm);
    if (-1 == status) {
        fail("pclose", true);
    }
    if (!WIFEXITED(status)) {
        fail("nm failed to terminate normally", false);
    }
    if (0 != WEXITSTATUS(status)) {
        failf("nm failed with status %d", WEXITSTATUS(status));
    }

    exit(EXIT_SUCCESS);
}

static void __attribute__((noreturn))
version(const char *unused)
{
    (void) unused;
    dprintf(STDOUT_FILENO,
            "version \t" VERSION "\n"
            "compiler\t" COMPILER "\n"
            "built   \t" __DATE__ ", " __TIME__ "\n") ;
    exit(EXIT_SUCCESS);
}

/* Print a usage string and terminate */
static void __attribute__((noreturn))
usage(const char *argv0)
{
    assert(!is_lib);

    dprintf(STDERR_FILENO, USAGE, argv0);
    dprintf(STDERR_FILENO, "Flags:\n"
            "\t-l\tList all supported functions\n"
            "\t-e FUNC\tList all errno values for FUNC\n"
            "\t-c EXEC\tList all tripable functions in EXEC\n"
#ifndef NDEBUG
            "\t-d\tPrint debugging information\n"
#endif
            "\t-V\tPrint version and build information\n"
            "\t-h\tPrint this message\n");
    exit(EXIT_SUCCESS);
}

int __attribute__((weak))
main(int argc, char *argv[])
{
    _Static_assert(0 < LENGTH(names), "The names array is empty");

    argv0 = argv[0];
    is_lib = false;

    /* If the environmental variable is set, we are currently being
     * invoked instead of the actual main function.  As this is not
     * intended, we abort execution immediately. */
    char *conf = getenv(ENVCONFNAME);
    if (conf) {
        dprintf(STDERR_FILENO, "Don't trip me\n");
        exit(EXIT_FAILURE);
    }

    struct option {
        void (*fn)(const char *); char *arg;
    } options[1 << CHAR_BIT] = {
    ['l'] = { list,          NULL    },
    ['e'] = { list_errors,   NULL    },
    ['c'] = { check_exec,    NULL    },
    ['V'] = { version,       NULL    },
    ['h'] = { usage,         argv[0] },
    };
    struct option *choice = NULL;

    /* Otherwise we are being invoked to wrap an actual call.  Let us *
     * start by parsing the command line. */
    int opt;
    while ((opt = getopt(argc, argv, "dle:c:Vh")) != -1) {
        switch (opt) {
        case 'd':
#ifndef NDEBUG
            debug_mode = true;
#else
            fail("trip not build with debugging option", false);
#endif
            break;
        default:
            if (NULL == options[opt].fn) {
                exit(EXIT_FAILURE); /* "invalid option" */
            }
            if (NULL != choice) {
                fail("contradictory flags", false);
            }

            choice = &options[opt];
            if (choice->arg == NULL) {
                choice->arg = optarg;
            }
        }
    }
    if (choice != NULL) {
        /* if we have selected a specific mode of operation, the
         * remaining flags will be ignored.  To avoid accidentally
         * passing too many arguments that are then silently ignored,
         * we instead remind the user of how to use Trip. */
        if (optind < argc) {
            usage(argv[0]);
        }

        assert(NULL != choice->fn);
        choice->fn(choice->arg);
    }

    if (optind >= argc) {
        usage(argv[0]);
    }

    char *entry = NULL, *s;
    entry = strtok_r(argv[optind++], ",", &s);
    do {
        enter(entry);
    } while ((entry = strtok_r(NULL, ",", &s)));

    if (optind >= argc) {
        usage(argv[0]);
    }

    /* Construct configuration */
    conf = NULL;
    size_t size = 0;
    FILE *h = open_memstream(&conf, &size);
    if (NULL == h) {
        fail("open_memstream", true);
    }

    if (0 > fprintf(h, "%s=", ENVCONFNAME)) {
        fail("printf", true);
    }
    if (debug_mode) {
        if (EOF == putc('D', h)) {
            fail("putc", true);
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        if (0 > fprintf(h, "%s" GS "%a" GS "%x" RS,
                        entries[i].name, entries[i].chance,
                        entries[i].error)) {
            fail("printf", true);
        }
    }
    fclose(h);

    /* Get path to the shared library */
    for (size_t size = 1<<6; ; size += 1<<6) {
        char preload[size];
        debugf("resolving /proc/self/exe with %lu byte", size);

        strncpy(preload, "LD_PRELOAD=", sizeof preload);
        size_t prefix = strlen(preload);
#ifdef __linux__
        ssize_t ret = readlink("/proc/self/exe", preload + prefix,
                               sizeof preload - prefix);

        assert(-1 <= ret);
        if (-1 == ret) {
            fail("readlink", true);
        }
        if (sizeof preload - prefix == (size_t) ret) {
            /* the memory automatically allocated in PRELOAD was not
             * sufficient to store the resolved file name.  We will
             * proceed to allocate more memory and and attempt
             * resolving the symbolic link path again. */
            continue;
        }
        assert('\0' == preload[prefix + (size_t)ret]);
#else
#error "System is not supported"
#endif

        execvpe(argv[optind], argv + optind, (char*[]) {preload, conf, NULL});
        fail("exec", true);
    }
}
