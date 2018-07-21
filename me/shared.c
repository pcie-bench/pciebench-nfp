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

/*
 * This file contains declarations which are shared between the master
 * ME code and the Worker ME code.  It needs to be #include'ed.
 */
#ifndef _PCIEBENCH_SHARED_C_
#define _PCIEBENCH_SHARED_C_

/*
 * Main memory location for the host to control the benchmarks..
 * The host code writes test parameters to @test_params, fills in the
 * DMA addresses for the host side buffers into @host_dma_addrs before
 * setting @test_ctrl to the test number defined in @pciebench_tests.
 * Once the test is finished, the NFP code writes the results to
 * @test_result and sets @test_ctrl to 0 to indicate to the host that
 * the test is finished.
 */
__export __cls volatile int32_t test_ctrl = 0;
__export __cls volatile struct test_params test_params;
__export __cls volatile struct test_result test_result;
__export __cls volatile uint64_t host_dma_addrs[PCIEBENCH_CHUNKS];

/*
 * The host writes the DMA addresses for each chunk of memory to
 * @host_dma_addrs.  Before a test is started these are copied into
 * local memory @chunk_dma_addrs for quicker access.
 */
__shared __lmem uint64_t chunk_dma_addrs[PCIEBENCH_CHUNKS];

/*
 * NFP buffer used for DMA
 */
__export __NFP_BUF_LOC volatile uint64_t nfp_buf[NFP_BUF_SZ64];

/*
 * Journal declaration for latency tests
 */
MEM_JOURNAL_DECLARE(PCIEBENCH_JOURNAL_RNUM, test_journal, PCIEBENCH_JOURNAL_SZ);

/*
 * Journal declaration for address debug
 */
MEM_JOURNAL_DECLARE(PCIEBENCH_DBG_RNUM,
                    debug_journal, PCIEBENCH_DBG_JOURNAL_SZ);

#endif /* _PCIEBENCH_SHARED_C_ */

/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*- */
