/*
 * Copyright (C) 2015 Rolf Neugebauer. All rights reserved.
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

#ifndef _PCIEBENCH_H_
#define _PCIEBENCH_H_

/**
 * CPP2PCIe Bar Configuration register to use
 */
#define PCIEBENCH_C2P_IDX 0

/**
 * PCIe Island to use (NFP-6000 only of course)
 */
#define PCIEBENCH_PCIE_ISL 0

/**
 * Maximum PCIe command transfer size
 */
#define PCIEBENCH_MAX_CMD_SZ 64


/**
 * Memory management:
 *
 * The host driver allocates a largish are of memory,
 * i.e. @PCIEBENCH_MAX_MEM, in chunks of @PCIEBENCH_CHUNK_SZ.  Each
 * chunk is contiguous in DMA space and @PCIEBENCH_CHUNK_SZ is the
 * largest we can allocate reliably on most kernels without boot time
 * reservations and the like.
 *
 * Some host code needs to write the start DMA address to
 * @host_dma_addrs so that the ME code knows which areas are
 * accessible.  The main ME code copies the DMA addresses into local
 * memory (@chunk_dma_addrs) before each test is run for quicker access.
 *
 * Tests typically operate over a "window" which is smaller or equal
 * to the total amount of host memory.  A window always starts at the
 * start of the first chunk.  If a window is larger than a chunk, the
 * second, third, and so forth, chunks are used.
 *
 * It is the responsibility of the host code to ensure that each chunk
 * can be addressed with a single CPP2PCIe BAR and it is 4K aligned.
 *
 *
 * Since the address calculation for DMA addresses can be fairly
 * expensive, especially for random accesses and non-power-of-2
 * windows sizes, the ME code calculates a list of DMA address the ME
 * code accesses to a large array in NFP memory prior to running the
 * test.  The array is called @dma_addrs and is of size
 * @PCIEBENCH_ADDR_ARRAY_SZ.  They map a unit index to a DMA address.
 *
 * An array element is 64bit, with the low 40bit containing the host
 * DMA address and the top byte being the chunk index.
 */

/**
 * Macros for host buffers
 *
 * The host allocates a number of memory buffers in @PCIEBENCH_CHUNK_SZ
 * sized chunks up to a total size of @PCIEBENCH_MAX_MEM, resulting in
 * @PCIEBENCH_CHUNKS chunks.  Each chunk is contiguous in DMA address
 * space.
 *
 * NOTE: @PCIEBENCH_CHUNK_SZ must be a power of two.
 * NOTE: These need to be kept in sync with the kernel module.
 */
#define PCIEBENCH_MAX_MEM (64 * 1024 * 1024)
#define PCIEBENCH_CHUNK_SZ (4 * 1024 * 1024)
#define PCIEBENCH_CHUNKS (PCIEBENCH_MAX_MEM / PCIEBENCH_CHUNK_SZ)

#define PCIEBENCH_CHUNK_SZ_mask (PCIEBENCH_CHUNK_SZ - 1)

/**
 * The host writes DMA address to an array (@dma_addrs) of size
 * PCIEBENCH_ADDR_ARRAY_SZ.
 */
#define PCIEBENCH_ADDR_ARRAY_SZ (PCIEBENCH_MAX_MEM / 64)
#define PCIEBENCH_ADDR_ARRAY_SZ_mask (PCIEBENCH_ADDR_ARRAY_SZ - 1)

/**
 * Queue indices to use for journaling.
 *
 * Some tests may journal data and can use @PCIEBENCH_JOURNAL_RNUM Q for
 * it. @PCIEBENCH_JOURNAL_SZ defines the number of 32bit entries in the
 * journal.
 * @debug_journal is for address debugging.
 */
#define PCIEBENCH_JOURNAL_RNUM 1
#define PCIEBENCH_JOURNAL_SZ (16 * 1024 * 1024)

MEM_JOURNAL_DECLARE_EXT(test_journal);


#define PCIEBENCH_DBG_RNUM 2
#define PCIEBENCH_DBG_JOURNAL_SZ (16 * 1024 * 1024)

MEM_JOURNAL_DECLARE_EXT(debug_journal);

/**
 * How much data should be transferred for bandwidth tests.
 *
 * This is defined in multiples of maximum host memory size
 */
#define PCIEBENCH_BW_MAX_TRANS (31 * PCIEBENCH_MAX_MEM)

/**
 * Iterations done for latency tests
 */
#define PCIEBENCH_LAT_TRANS (2 * 1024 * 1024)

/**
 * Number of DMAs to perform for Bandwidth tests
 */
#define PCIEBENCH_BW_TRANS (8 * 1024 * 1024)

/**
 * Local memory cache of host DMA addresses
 */
__shared __lmem extern uint64_t chunk_dma_addrs[PCIEBENCH_CHUNKS];

/**
 * Signal used for orchestrating DMA BW workers
 */
#define PCIEBENCH_CTRL_SIGNO 15

/**
 * Maximum number of worker MEs
 */
#ifdef __NFP_IS_3200
#define PCIEBENCH_LAST_WORKER_ME 7
#else
#define PCIEBENCH_LAST_WORKER_ME 11
#endif

/**
 * Memory for NFP side buffer
 * We use CTM on the 6k and dram memory on the 3200. Size must be
 * power of two to allow masking of address offsets on the card.
 */
#ifdef __NFP_IS_3200
#define __NFP_BUF_LOC __mem
#else
#define __NFP_BUF_LOC __ctm_n(4) __shared __declspec(scope(global))
#endif
#define NFP_BUF_SZ (8 * 1024)
#define NFP_BUF_SZ64 (NFP_BUF_SZ / 8)

__export __NFP_BUF_LOC extern volatile uint64_t nfp_buf[NFP_BUF_SZ64];

/**
 * Initialise the state for address calculation
 * @win_sz    Size of the window
 * @trans_sz  Transaction size
 * @h_off     Host offset
 * @flags     Flags (id Host offset
 *
 * This function pre-calculates and array of DMA addresses based on
 * the parameters. @dma_addr_from_idx() then becomes a simple array
 * lookup.
 */
__intrinsic void dma_addr_init(uint32_t win_sz, uint32_t sz,
                               uint32_t off, uint32_t flags);

/**
 * Translate a unit index into a DMA address
 * @idx         Unit index
 * @addr_hi     Return high bits of DMA address
 * @addr_lo     Return low bits of DMA address
 * @chunk_idx   Return the chunk index used
 *
 * This function relies on the array set up in @dma_addr_init().
 */
__intrinsic void dma_addr_from_idx(uint32_t idx,
                                   __gpr uint32_t *addr_hi,
                                   __gpr uint32_t *addr_lo,
                                   __gpr uint32_t *chunk_idx);

/**
 * Attempt to thrash or warm the host cache.  For thrashing the host
 * cache we write randomly to the entire host buffers.  For warming,
 * use sequential writes to the host memory in the window.
 *
 * NOTE: These functions use @dma_addr_init() and @dma_addr_from_idx()
 * so any benchmarking code should call these functions *before*
 * setting up the benchmark specific local state.
 */
__intrinsic void host_trash_cache(void);
__intrinsic void host_warm_cache(int win_sz);


/**
 * Tests support by the performance code
 *
 * See the prototype definition referenced in the comment for test details.
 */
enum pciebench_tests {
    LAT_CMD_RD   =   1,  /* see @lat_cmd */
    LAT_CMD_WRRD =   2,  /* see @lat_cmd */
    LAT_DMA_RD   =   3,  /* see @lat_dma */
    LAT_DMA_WRRD =   4,  /* see @lat_dma */
    BW_DMA_RD    =   5,  /* see @bw_dma */
    BW_DMA_WR    =   6,  /* see @bw_dma */
    BW_DMA_RW    =   7,  /* see @bw_dma */
};


/**
 * Each test may have up to 4 parameters.  See test documentation for details
 */
struct test_params {
    uint32_t p0;
    uint32_t p1;
    uint32_t p2;
    uint32_t p3;
    uint32_t p4;
};


/**
 * Result for a test.
 *
 * @rX are 4 generic result values. See test documentation for details.
 */
struct test_result {
    uint32_t start_hi;          /*< Top 32 bit of ME timestamp at start */
    uint32_t start_lo;          /*< Bottom 32 bit of ME timestamp at start */
    uint32_t end_hi;            /*< Top 32 bit of ME timestamp at end */
    uint32_t end_lo;            /*< Bottom 32 bit of ME timestamp at end */

    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
};


/**
 * Flags for the latency tests
 */
enum lat_flags {
    LAT_FLAGS_WARM        = 1 << 0,  /*< Warm the window before the test */
    LAT_FLAGS_THRASH      = 1 << 1,  /*< Clean the buffers before the test */
    LAT_FLAGS_RANDOM      = 1 << 2,  /*< Random access */
    LAT_FLAGS_LONG        = 1 << 3,  /*< Run longer than default */
    LAT_FLAGS_RESERVED    = 1 << 31
};


/**
 * Read/write data from the host using the PCIe command and measure the time.
 *
 * @param p     Parameters/arguments for the test
 * @param r     Results returned
 * @param test  Which test to run (see below)
 * @returns     0 on success, negative on error
 *
 * This function implements two tests: @LAT_CMD_RD and @LAT_CMD_WRRD
 * depending on which one is passed in as @test.  @test must be a
 * constant so that only the right code path is selected during
 * compilation.
 *
 * The test parameters are as follows:
 * @p0:         Flags (see below)
 * @p1:         Transaction size
 * @p2:         Window size to operate on
 * @p3:         Offset from a host cacheline start for the read/write
 * @p4:         Not used
 *
 * This functions measures the latency of PCIe commands, either a
 * simple read (@LAT_CMD_RD) or a write to a host memory location
 * followed by a read from the same location (@LAT_CMD_WRRD).  A time
 * stamp is taken just before and after the command(s) and the
 * difference is written to the journal.  The test thus measures the
 * latency of a PCIe read or a PCIe write followed by a PCIe read.
 *
 * By default the DMA addresses used are always at the start of a host
 * cache line irrespective of the transaction size (@p1) but a local
 * offset can be specified using @p3.  The test cycles through all
 * host cache lines within the window before going back to the start
 * of the window.  A single CPP2PCIe BAR is used and it is only
 * reconfigured when necessary, i.e., when moving to a new chunk.
 *
 * By default, host addresses are accessed sequential.  If
 * @LAT_FLAGS_RANDOM is set, random host offsets (cacheline aligned
 * with offset) is selected.
 *
 * By default @PCIEBENCH_LAT_TRANS transaction are performed, ensuring
 * that even for the largest window size, each host cache line is hit
 * at least twice.  When @LAT_FLAGS_LONG is set,
 * @PCIEBENCH_JOURNAL_SZ transactions are performed, filling the entire
 * journal.
 *
 * If the flag @LAT_FLAGS_WARM is set, the code writes full host
 * cachelines to the entire window, starting from the start, before
 * the actual test.  Depending on the host caching and PCIe
 * architecture, this may warm any caches used on the host for PCIe
 * transactions.
 *
 * If the flag @LAT_FLAGS_THRASH is set, the code writes full cache
 * lines to all chunks of host memory, starting with the first chunk.
 * Since the host side memory is at least twice the size of any cache
 * on the host, this should ensure that the caches are clean from any
 * addresses the MEs may access.
 *
 * In the result struct, the start/end time values are set outside the
 * main loop, so they represent the total time spent on the test. The
 * other results are set as follows:
 * @r0:         Number of PCIe transactions (items in journal)
 *
 * Using the start/end timestamps in the result structure together
 * with @r0, one can calculate the average cost of each loop.  Note,
 * this average is obviously higher than the average computed from the
 * individual measurements in the journal as the former also contains
 * the time spent on calculating the next address as well as any BAR
 * re-configurations.
 */
__intrinsic int32_t cmd_lat(__gpr struct test_params *p,
                            __gpr struct test_result *r, int test);

/**
 * Read/write data from the host using the DMA engine and measure the time.
 *
 * @param p     Parameters/arguments for the test
 * @param r     Results returned
 * @param test  Which test to run (see below)
 * @returns     0 on success, negative on error
 *
 * This function implements two tests: @LAT_DMA_RD and @LAT_DMA_WRRD
 * depending on which one is passed in as @test.  @test must be a
 * constant so that only the right code path is selected during
 * compilation.
 *
 * This test is identical to @cmd_lat() except it uses the DMA engines
 * instead of the PCIe command.
 *
 * The test parameters are as follows:
 * @p0:         Flags (see below)
 * @p1:         Transaction size
 * @p2:         Window size to operate on
 * @p3:         Offset from a host cacheline start for the read/write
 * @p4:         Offset from start of NFP buffer
 */
__intrinsic int32_t dma_lat(__gpr struct test_params *p,
                            __gpr struct test_result *r, int test);



__intrinsic int32_t dma_bw(__gpr struct test_params *p,
                           __gpr struct test_result *r, int test);

/* Entry function for DMA worker threads */
void dma_bw_worker(void);

#endif /* _PCIEBENCH_H_ */
