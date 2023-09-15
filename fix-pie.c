/* REMOVE THE PIE-FLAG FROM AN EXECUTABLE
 *
 * Copyright (c) 2020  Yubo Xie <xyb@xyb.name>
 * Copyright (c) 2023  Philip Kaludercic
 *
 * Permission  is  hereby  granted,  free of  charge,  to  any  person
 * obtaining  a copy  of  this software  and associated  documentation
 * files   (the  “Software”),   to  deal   in  the   Software  without
 * restriction, including without limitation  the rights to use, copy,
 * modify, merge, publish, distribute,  sublicense, and/or sell copies
 * of the  Software, and  to permit  persons to  whom the  Software is
 * furnished to do so, subject to the following conditions:
 *
 * The  above copyright  notice and  this permission  notice shall  be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE  IS PROVIDED  “AS IS”, WITHOUT  WARRANTY OF  ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT  NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY,   FITNESS    FOR   A   PARTICULAR    PURPOSE   AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES  OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT,  TORT OR OTHERWISE, ARISING FROM, OUT  OF OR IN
 * CONNECTION WITH  THE SOFTWARE OR THE  USE OR OTHER DEALINGS  IN THE
 * SOFTWARE.
 *
 **********************************************************************
 *
 * Based on the following script:
 *
 *  https://gist.github.com/xieyubo/6820491646f9d01980a7eadb16564ddf.
 *
 * This is necessary to address a behaviour change in
 * glibc>=2.30. dlopen() will fail if the file is position independent
 * executable.  See
 *
 *   https://bugzilla.redhat.com/show_bug.cgi?id=1764223
 *
 * for more details.  We need to address this because the "trip"
 * executable is also used as a shared-library.
 *
 * Consider using GNU Poke (https://www.jemarch.net/poke) instead at
 * some point.
 **/

#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    struct stat stat;
    Elf64_Ehdr *ehdr;           /* ELF header */
    Elf64_Shdr *shdr;           /* section header */
    Elf64_Dyn *dyn;             /* "dynamic" section */
    int fd, i, j;
    char *mem;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <executable-elf64-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (-1 == (fd = open(argv[1], O_RDWR))) {
        perror("open");
        return EXIT_FAILURE;
    }
    if (-1 == fstat(fd, &stat)) {
        perror("fstat");
        close(fd);
        return EXIT_FAILURE;
    }

    mem = mmap(NULL, stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == mem) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    ehdr = (Elf64_Ehdr *) mem;

    /* Check if we actually have an ELF file. */
    if (0 != strncmp((char *) ehdr->e_ident, ELFMAG, SELFMAG)) {
        fprintf(stderr, "Not a valid ELF file (%s)\n", ehdr->e_ident);
        return EXIT_FAILURE;
    }

    /* Attempt to locate "dynamic" section by iterating through the
     * section indicated in the ELF header. */
    /* shdr = (Elf64_Shdr *)(mem + ehdr->e_shoff); */
    for (i = 0; i < ehdr->e_shnum; ++i) {
        shdr = (Elf64_Shdr *) (mem + ehdr->e_shoff + i * ehdr->e_shentsize);
        if (shdr->sh_type != SHT_DYNAMIC) {
            continue;           /* we are looking for the "dynamic" section */
        }

#define GETDYN(n) (Elf64_Dyn *) (mem + shdr->sh_offset + n * sizeof(Elf64_Dyn))

        /* Iterate through the section until we find the dynamic entry
         * with the "DT_FLAGS_1" tag.  This is where the PIE flag is
         * stored. */
        for (j = 0; dyn = GETDYN(j), dyn->d_tag != 0; ++j) {
            if (dyn->d_tag == DT_FLAGS_1) {
                if (!(dyn->d_un.d_val & DF_1_PIE)) {
                    fprintf(stderr, "The PIE flag was not set.\n");
                    return EXIT_FAILURE;
                } else {
                    dyn->d_un.d_val &= ~DF_1_PIE;       /* clear the flag */
                }

                if (close(fd)) {
                    perror("close");
                }
                return EXIT_SUCCESS;    /* mors mihi lucrum */
            }
        }
    }

    fprintf(stderr, "Failed to unset PIE flag.\n");
    return EXIT_FAILURE;
}
