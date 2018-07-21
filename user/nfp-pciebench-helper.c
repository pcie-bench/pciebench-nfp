/*
 * Copyright (C) 2015-2018 Rolf Neugebauer. All rights reserved.
 * Copyright (C) 2015 Netronome Systems, Inc.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include <errno.h>

#include <nfp.h>
#include <nfp_nffw.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*(arr)))
#endif

void usage(const char *program)
{
    printf("Usage: "
           "%s [options]\n"
           "\n"
           "Start a NFP PCIe benchmark after thrashing/warming the cache.\n"
           "Returns after the test finished\n"
           "\n"
           "Note, this program assumes that all the tests parameters have\n"
           "already been set up.\n"
           "\n"
           "  -n NFP        NFP number.\n"
           "  -c TEST_CTRL  Symbol name for test control.\n"
           "  -t TEST       Test to run.\n"
           "  -w WIN        Warm a window of WIN size.\n"
           "  -h            Show this help message and exit.\n"
           "\n", program);
    exit(1);
}

/*
 * Before starting a test we aim thrash the cache by randomly
 * writing to elements in a 64MB large array.
 */
static uint64_t large_array[8 * 1024 * 1024];

static void
thrash_cache(void)
{
    int i, r;

    for (i = 0; i < 4 * ARRAY_SIZE(large_array); i++) {
        r = rand();
        large_array[r % ARRAY_SIZE(large_array)] = (uint64_t)i * r;
    }
}


/* Warm the host buffers for a given window size. The window size is
 * rounded up to the nearest full page. */
static void
warm_cache(int nfp_no, int win_sz)
{
    char fn[256];
    int fd;
    int num_pages;
    uint32_t page[1024];
    int i, j;

    snprintf(fn, sizeof(fn), "/proc/pciebench_buffer-%d", nfp_no);

    fd = open(fn, O_WRONLY | O_SYNC);
    if (fd < 0) {
        perror("Failed to open host buffer file");
        exit(1);
    }

    /* Fill page with a unique pattern */
    for (i = 0; i < ARRAY_SIZE(page); i++)
        page[i] = 0xf00d0000 + 1;

    /* Round up to the next page */
    num_pages = (win_sz - 1) / 4096 + 1;

    /* Write win_sz worth of pages to the start of the host
     * buffer. repeat a number of times. */
    for (i = 0; i < 4; i++) {
        lseek(fd, 0, SEEK_SET);
        for (j = 0; j < num_pages; j++)
            write(fd, page, sizeof(page));
    }

    close(fd);
}

int
main(int argc, char *argv[])
{
    char *cp;
    int r;
    int opt_nfp = 0, opt_test = -1, opt_win = 0;
    char opt_ctrl[256];

    struct nfp_device *nfp;
    const struct nfp_rtsym *sym;

    while ((r = getopt(argc, argv, "n:c:t:w:h")) != -1) {
        switch(r) {
        case 'n':
            opt_nfp = strtoul(optarg, &cp, 0);
            if ((cp == optarg) || (*cp != 0))
                usage(argv[0]);
            break;

        case 'c':
            strncpy(opt_ctrl, optarg, sizeof(opt_ctrl));
            break;

        case 't':
            opt_test = strtoul(optarg, &cp, 0);
            if ((cp == optarg) || (*cp != 0))
                usage(argv[0]);
            break;

        case 'w':
            opt_win = strtoul(optarg, &cp, 0);
            if ((cp == optarg) || (*cp != 0))
                usage(argv[0]);
            break;

        default:
            usage(argv[0]);
            break;
        }
    }

    if (opt_test == -1)
        usage(argv[0]);


    nfp = nfp_device_open(opt_nfp);
    if (!nfp) {
        perror("Open NFP device");
        return -1;
    }

    sym = nfp_rtsym_lookup(nfp, opt_ctrl);
    if (!sym) {
        perror("Lookup symbol");
        return -1;
    }

    /* Always thrash the cache */
    thrash_cache();

    /* Warm the host buffers if requested */
    if (opt_win)
        warm_cache(opt_nfp, opt_win);

    /* start the test */
    nfp_rtsym_write(nfp, sym, &opt_test, sizeof(opt_test), 0);

    /* Poll for the test to finish */
    while (opt_test > 0) {
        sleep(2);
        nfp_rtsym_read(nfp, sym, &opt_test, sizeof(opt_test), 0);
    }

    return 0;
}

/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*- */
