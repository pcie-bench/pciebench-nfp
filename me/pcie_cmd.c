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

#include <stdint.h>

#include "compat.h"
#include "libnfp.h"
#include "pciebench.h"

/*
 * Execute the @LAT_CMD_RD and @LAT_CMD_WRRD tests
 */
__intrinsic int32_t
cmd_lat(__gpr struct test_params *p, __gpr struct test_result *r, int test)
{
    __xwrite uint32_t w_data[16];
    __xread uint32_t r_data[16];
    SIGNAL w_sig, r_sig;

    __gpr uint32_t arg_trans_sz, arg_hoff, arg_win, arg_flags;

    __gpr uint32_t trans, max_trans = PCIEBENCH_LAT_TRANS;
    __gpr uint32_t addr_hi, addr_lo;
    __gpr uint32_t chunk_idx, old_chunk_idx;

    __gpr uint32_t t0, t1;
    __gpr int i, ret = 0;

    arg_flags = p->p0;
    arg_trans_sz = p->p1;
    arg_win = p->p2;
    arg_hoff = p->p3;

    /* Sanity checks */
    if ((arg_trans_sz + arg_hoff > 4096) ||
        (arg_trans_sz + arg_hoff > arg_win)) {
        ret = -1;
        goto out;
    }

    /* Init the addresses array */
    dma_addr_init(arg_win, arg_trans_sz, arg_hoff, arg_flags);

    /* Thrash the cache if requested */
    if (arg_flags & LAT_FLAGS_THRASH)
        host_trash_cache();

    if (arg_flags & LAT_FLAGS_LONG)
        max_trans = PCIEBENCH_JOURNAL_SZ;

    /* Warm the window if requested */
    if (arg_flags & LAT_FLAGS_WARM)
        host_warm_cache(arg_win);

    /* Set up first address */
    dma_addr_from_idx(0, &addr_hi, &addr_lo, &old_chunk_idx);
    pcie_c2p_barcfg(PCIEBENCH_PCIE_ISL, PCIEBENCH_C2P_IDX, addr_hi, addr_lo, 0);

    r->start_lo = ts_lo_read();
    r->start_hi = ts_hi_read();

    for (trans = 0; trans < max_trans; trans++) {

        /* For write tests create a unique test pattern */
        if (test == LAT_CMD_WRRD)
            for (i = 0; i < arg_trans_sz / sizeof(uint32_t); i++)
                w_data[i] = 0x0000beef | ((0xffff - trans) << 16);

        t0 = ts_lo_read();

        switch (test) {

        case LAT_CMD_RD:
            __pcie_read(r_data, PCIEBENCH_PCIE_ISL, PCIEBENCH_C2P_IDX,
                        addr_hi, addr_lo, arg_trans_sz, 64, sig_done, &r_sig);
            wait_for_all(&r_sig);
            __implicit_read(r_data);
            break;

        case LAT_CMD_WRRD:
            __pcie_write(w_data, PCIEBENCH_PCIE_ISL, PCIEBENCH_C2P_IDX,
                         addr_hi, addr_lo, arg_trans_sz, 64, sig_done, &w_sig);
            __pcie_read(r_data, PCIEBENCH_PCIE_ISL, PCIEBENCH_C2P_IDX,
                        addr_hi, addr_lo, arg_trans_sz, 64, sig_done, &r_sig);
            wait_for_all(&w_sig, &r_sig);
            __implicit_read(w_data);
            __implicit_read(r_data);
            break;

        default:
            ret = -2;
            goto out;
        }

        t1 = ts_lo_read();
        MEM_JOURNAL_FAST(test_journal, t1 - t0);

        MEM_JOURNAL_FAST(debug_journal, addr_hi);
        MEM_JOURNAL_FAST(debug_journal, addr_lo);

       dma_addr_from_idx(trans, &addr_hi, &addr_lo, &chunk_idx);
        if (chunk_idx != old_chunk_idx) {
            pcie_c2p_barcfg(PCIEBENCH_PCIE_ISL, PCIEBENCH_C2P_IDX,
                            addr_hi, addr_lo, 0);
            old_chunk_idx = chunk_idx;
        }
    }

    r->end_lo = ts_lo_read();
    r->end_hi = ts_hi_read();
    r->r0 = trans;
    r->r1 = 0;
    r->r2 = 0;
    r->r3 = 0;

out:
    return ret;
}
