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

#include <stdint.h>

#include "compat.h"
#include "libnfp.h"
#include "pciebench.h"

/*
 * Utility functions
 */
__export __emem __align(64) volatile uint64_t \
    dma_addrs[PCIEBENCH_ADDR_ARRAY_SZ];


__intrinsic void
dma_addr_init(uint32_t win_sz, uint32_t trans_sz,
              uint32_t h_off, uint32_t flags)
{
    __gpr uint32_t addr_hi, addr_lo, chunk_off;
    __gpr uint32_t chunk_idx, old_chunk_idx;
    __gpr uint32_t units_in_win;
    __gpr uint32_t lin_addr;
    __gpr uint64_t dma_addr;
    __gpr uint32_t unit_sz;
    __gpr uint32_t trans;
    __gpr uint32_t avail;
    __gpr uint32_t add;
    __gpr int idx;

    unit_sz = roundup64(trans_sz + h_off);
    units_in_win = win_sz / unit_sz;

    add = 0;
    for (idx = 0; idx < PCIEBENCH_ADDR_ARRAY_SZ; idx++) {
        for (;;) {

            if (flags & LAT_FLAGS_RANDOM)
                trans = local_csr_read(local_csr_pseudo_random_number);
            else
                trans = idx + add;

            /* Calculate linear address based on index */
            lin_addr = trans % units_in_win;
            lin_addr *= unit_sz;
            lin_addr += h_off;

            /* Check that the transaction would not cross a 4k boundary */
            avail = 0x1000 - (lin_addr & 0xfff);
            if (avail >= trans_sz)
                break;
            else
                add += 1;
        }

        /* Convert linear address to DMA address */
        chunk_idx = lin_addr >> __log2(PCIEBENCH_CHUNK_SZ);
        chunk_off = lin_addr & PCIEBENCH_CHUNK_SZ_mask;

        dma_addr = chunk_dma_addrs[chunk_idx];
        dma_addr += chunk_off;

        /* Splice in the chunk index and write to array */
        dma_addr |= (uint64_t)chunk_idx << 56;
        dma_addrs[idx] = dma_addr;
    }
}

__intrinsic void
dma_addr_from_idx(uint32_t idx,
                  __gpr uint32_t *addr_hi, __gpr uint32_t *addr_lo,
                  __gpr uint32_t *chunk_idx)
{
    __gpr uint64_t dma_addr;

    dma_addr = dma_addrs[idx & PCIEBENCH_ADDR_ARRAY_SZ_mask];

    *addr_lo = dma_addr & 0xffffffff;
    *addr_hi = dma_addr >> 32;

    *chunk_idx = (*addr_hi >> 24) & 0xff;
    *addr_hi = *addr_hi & 0xffffff;
}

/*
 * Write a pattern to a region of @sz size in host memory. Allow
 * random and sequential patterns.
 *
 * Note there is no need to check the return value of
 * @dma_addr_from_idx() because we only use aligned 64B transactions.
 */
__intrinsic static void
write_region(__gpr uint32_t win_sz, const uint32_t pattern, const int rand)
{
    __xwrite uint32_t w_data[16];
    __gpr uint32_t idx, trans, num_trans;
    __gpr uint32_t lin_addr;
    __gpr uint32_t addr_hi, addr_lo, chunk_off;
    __gpr uint32_t chunk_idx, old_chunk_idx;
    __gpr uint32_t i;
    __gpr int ret;
    SIGNAL w_sig;

    old_chunk_idx = 0;
    addr_hi = chunk_dma_addrs[0] >> 32;
    addr_lo = chunk_dma_addrs[0] & 0xffffffff;

    pcie_c2p_barcfg(PCIEBENCH_PCIE_ISL, PCIEBENCH_C2P_IDX, addr_hi, addr_lo, 0);

    num_trans = 2 * (win_sz / sizeof(w_data));
    for (trans = 0; trans < num_trans; trans++) {

        /* Different content per cache line */
        for (i = 0; i < ARRAY_SIZE(w_data); i++)
            w_data[i] = (pattern & 0xffff0000) | (trans & 0xffff);

        __pcie_write(w_data, PCIEBENCH_PCIE_ISL, PCIEBENCH_C2P_IDX,
                     addr_hi, addr_lo, sizeof(w_data), 64, sig_done, &w_sig);
        wait_for_all(&w_sig);
        __implicit_read(w_data);


        if (rand)
            idx = local_csr_read(local_csr_pseudo_random_number);
        else
            idx = trans + 1;

        lin_addr = idx << 6;
        lin_addr = lin_addr % win_sz;

        /* Convert Linear DMA address into host size chunk address/offset */
        chunk_idx = lin_addr >> __log2(PCIEBENCH_CHUNK_SZ);
        chunk_off = lin_addr & PCIEBENCH_CHUNK_SZ_mask;

        addr_hi = chunk_dma_addrs[chunk_idx] >> 32;
        addr_lo = chunk_dma_addrs[chunk_idx] & 0xffffffff;
        addr_lo += chunk_off;

        /* Reprogram BAR if necessary */
        if (chunk_idx != old_chunk_idx) {
            pcie_c2p_barcfg(PCIEBENCH_PCIE_ISL, PCIEBENCH_C2P_IDX,
                            addr_hi, addr_lo, 0);
            old_chunk_idx = chunk_idx;
        }
    }
}

__intrinsic void
host_trash_cache(void)
{
    write_region(PCIEBENCH_MAX_MEM, 0xdead0000, 1);
}

__intrinsic void
host_warm_cache(int win_sz)
{
    write_region(win_sz, 0xcafe0000, 0);
}
