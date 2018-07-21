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

/* Location where host writes test parameters */
__import __cls volatile int32_t test_ctrl;
__import __cls volatile struct test_params test_params;

/* Global, shared test parameters, mostly for DMA BW tests */
__shared __gpr static uint32_t test_no;

__shared __gpr static uint32_t arg_flags;
__shared __gpr static uint32_t arg_trans_sz;
__shared __gpr static uint32_t arg_win;
__shared __gpr static uint32_t arg_hoff;
__shared __gpr static uint32_t arg_doff;

/* CLS variable to hold number of DMAs to perform */
__export __shared __cls uint32_t num_dma_trans;


/*
 * Fill out all the common parts of the DMA command structure, plus
 * other setup required for DMA engines tests.
 *
 * Once setup, the caller only needs to patch in the PCIe address and
 * is ready to go.
 */
__intrinsic static void
pcie_dma_setup(__gpr struct nfp_pcie_dma_cmd *cmd,
               int signo, uint32_t len, int d_off)
{
    unsigned int meid = __MEID;

    /* Zero the descriptor. Same size for 3200 and 6000 */
    cmd->__raw[0] = 0;
    cmd->__raw[1] = 0;
    cmd->__raw[2] = 0;
    cmd->__raw[3] = 0;

#if __NFP_IS_3200
    {
        union pcie_dma_completion cmpl;

        /* Signalling setup */
        meid += 0x8;
        cmpl.cl = meid >> 4;
        cmpl.me = meid & 0xf;
        cmpl.ctx = __ctx();
        cmpl.signo = signo;

        /* CPP related fields */
        cmd->cpp_target = 7;     /* MU space */
        cmd->cpp_target64 = 1;
        cmd->token = 0;
        cmd->completion = cmpl.completion;

        cmd->cpp_addr_hi = 0;
        cmd->cpp_addr_lo = (uint32_t)((uint64_t)nfp_buf & 0xffffffff) + d_off;
        cmd->len = len;
    }
#else
    {
        struct nfp_pcie_dma_cfg cfg;
        __xwrite struct nfp_pcie_dma_cfg cfg_wr;
        unsigned int mode_msk_inv;
        unsigned int mode;


        /* We just write config register 0 and 1. no one else is using them */
        cfg.__raw = 0;
        cfg.target_64_even = 1;
        cfg.cpp_target_even = 7;
        cfg.target_64_odd = 1;
        cfg.cpp_target_odd = 7;

        cfg_wr = cfg;
        pcie_dma_cfg_set_pair(0, 0, &cfg_wr);

        /* Signalling setup */
        mode_msk_inv = ((1 << NFP_PCIE_DMA_CMD_DMA_MODE_shf) - 1);
        mode = (((meid & 0xF) << 13) | (((meid >> 4) & 0x3F) << 7) |
                ((__ctx() & 0x7) << 4) | signo);
        cmd->__raw[1] = ((mode << NFP_PCIE_DMA_CMD_DMA_MODE_shf) |
                         (cmd->__raw[1] & mode_msk_inv));

        cmd->cpp_token = 0;
        cmd->cpp_addr_hi = 0;
        cmd->cpp_addr_lo = (uint32_t)((uint64_t)nfp_buf & 0xffffffff) + d_off;
        /* On the 6k the length is length - 1 */
        cmd->length = len - 1;
    }
#endif
}

/*
 * Execute the @LAT_DMA_RD and @LAT_DMA_WRRD tests
 */
__intrinsic int32_t
dma_lat(__gpr struct test_params *p, __gpr struct test_result *r, int test)
{
    __gpr uint32_t trans, max_trans = PCIEBENCH_LAT_TRANS;
    __gpr uint32_t addr_hi, addr_lo;
    __gpr uint32_t unused;

    __gpr uint32_t t0, t1;
    __gpr int ret = 0;

    __gpr struct nfp_pcie_dma_cmd dma_cmd;
    __xwrite struct nfp_pcie_dma_cmd dma_cmd_wr;

    SIGNAL cmpl_sig, enq_sig;

    arg_flags = p->p0;
    arg_trans_sz = p->p1;
    arg_win = p->p2;
    arg_hoff = p->p3;
    arg_doff = p->p4;

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
    dma_addr_from_idx(0, &addr_hi, &addr_lo, &unused);

    /* Setup the generic parts of the DMA descriptor */
    pcie_dma_setup(&dma_cmd,
                   __signal_number(&cmpl_sig), arg_trans_sz, arg_doff);

    r->start_lo = ts_lo_read();
    r->start_hi = ts_hi_read();

    for (trans = 0; trans < max_trans; trans++) {

        dma_cmd.pcie_addr_hi = addr_hi;
        dma_cmd.pcie_addr_lo = addr_lo;
        dma_cmd_wr = dma_cmd;

        t0 = ts_lo_read();

        switch (test) {

        case LAT_DMA_RD:
            __pcie_dma_enq(0, &dma_cmd_wr, NFP_PCIE_DMA_FROMPCI_HI,
                           sig_done, &enq_sig);
            wait_for_all(&cmpl_sig, &enq_sig);
            break;

        case LAT_DMA_WRRD:
            /* DMA ToPCIE (PCIe write)*/
            __pcie_dma_enq(0, &dma_cmd_wr, NFP_PCIE_DMA_TOPCI_HI,
                           sig_done, &enq_sig);
            wait_for_all(&cmpl_sig, &enq_sig);

            /* DMA FromPCIE (PCIe read)*/
            __pcie_dma_enq(0, &dma_cmd_wr, NFP_PCIE_DMA_FROMPCI_HI,
                           sig_done, &enq_sig);
            wait_for_all(&cmpl_sig, &enq_sig);

            break;

        default:
            ret = -2;
            goto out;
        }

        t1 = ts_lo_read();
        MEM_JOURNAL_FAST(test_journal, t1 - t0);

        MEM_JOURNAL_FAST(debug_journal, addr_hi);
        MEM_JOURNAL_FAST(debug_journal, addr_lo);

        dma_addr_from_idx(trans, &addr_hi, &addr_lo, &unused);
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

/*
 * PCIe bandwidth tests
 *
 * For bandwidth tests, Context/Thread 0 (the master context) is
 * simply setting up the tests and waits for a number of worker
 * threads on this ME and other MEs to complete the work.
 *
 * The master context performs any warming/thrashing and sets-up the
 * address calcualtion state for worker threads in this ME.  It also
 * writes the number of DMAs to be performed to CLS.  After setting
 * everything up, it waits to be signalled that all DMAs have been
 * completed.
 *
 * Worker MEs, test_sub the CLS variable containing the number of DMAs
 * to complete.  This is atomic and value of the CLS variable is used
 * as the index for address calculation (for sequential access).  For
 * Read/Write tests, the value is also used to alternate between Read
 * and Write DMAs.  The worker contxt handling the last DMA signals
 * the master once the DMA completed.
 */


/*
 * Execute the @BW_RD, @BW_WR, and @BW_RW tests.
 *
 * Context 0 in the main app ME is not issuing any DMAs.
 */
__intrinsic int32_t
dma_bw(__gpr struct test_params *p, __gpr struct test_result *r, int test)
{
    __gpr uint32_t arg_win, max_trans = PCIEBENCH_BW_TRANS;
    __gpr int ret = 0;

    SIGNAL dma_ctrl_sig;
    __assign_relative_register(&dma_ctrl_sig, PCIEBENCH_CTRL_SIGNO);

    /* Copy test number and test argument into local registers shared
     * with the worker contexts. */
    test_no = test;
    arg_flags = p->p0;
    arg_trans_sz = p->p1;
    arg_win = p->p2;
    arg_hoff = p->p3;
    arg_doff = p->p4;

    /* Sanity checks */
    if ((arg_trans_sz + arg_hoff > 4096) ||
        (arg_trans_sz + arg_hoff > arg_win)) {
        ret = -1;
        goto out;
    }

    /* Set up address calculation state */
    dma_addr_init(arg_win, arg_trans_sz, arg_hoff, arg_flags);

    /* Thrash the cache if requested */
    if (arg_flags & LAT_FLAGS_THRASH)
        host_trash_cache();

    if (arg_flags & LAT_FLAGS_LONG)
        max_trans = PCIEBENCH_JOURNAL_SZ;

    /* Warm the window if requested */
    if (arg_flags & LAT_FLAGS_WARM)
        host_warm_cache(arg_win);

    /* Set up CLS atomic for the number of transactions */
    num_dma_trans = max_trans;

    /* record start time */
    r->start_lo = ts_lo_read();
    r->start_hi = ts_hi_read();

    /* Signal first worker */
    signal_next_ctx(PCIEBENCH_CTRL_SIGNO);

    /* Wait for the worker, who issued last DMA to signal us */
    wait_for_all(&dma_ctrl_sig);

    /* Record end time */
    r->end_lo = ts_lo_read();
    r->end_hi = ts_hi_read();
    r->r0 = max_trans;
    r->r1 = 0;
    r->r2 = 0;
    r->r3 = 0;

out:
    return ret;
}


void
dma_bw_worker(void)
{
    __gpr struct test_params params;
    __gpr uint32_t addr_hi, addr_lo;
    __gpr uint32_t unused;
    __gpr uint32_t trans;
    __gpr int read;

    __gpr int meid;

    __gpr struct nfp_pcie_dma_cmd dma_cmd;
    __xwrite struct nfp_pcie_dma_cmd dma_cmd_wr;

    SIGNAL cmpl_sig, enq_sig;
    SIGNAL dma_ctrl_sig;
    __assign_relative_register(&dma_ctrl_sig, PCIEBENCH_CTRL_SIGNO);

    meid = __ME();

    for (;;) {

        /* Wait for the start signal */
        wait_for_all(&dma_ctrl_sig);

        /* Context 0 on a worker ME reads in the parameter and copies
         * them to GPRs shared between all worker contexts. */
        if (ctx() == 0) {
            test_no = test_ctrl;

            params = test_params;
            arg_flags = params.p0;
            arg_trans_sz = params.p1;
            arg_win = params.p2;
            arg_hoff = params.p3;
            arg_doff = params.p4;
        }

        /* Ping the next context to start.
         * CTX 7 in each ME pings CTX 0 in the next ME. The last ME
         * does not need to ping anyone. */
        if (ctx() != 7)
            signal_next_ctx(PCIEBENCH_CTRL_SIGNO);
        else
            if ((meid & 0xf) != PCIEBENCH_LAST_WORKER_ME)
                signal_next_me(0, PCIEBENCH_CTRL_SIGNO);

        /* Setup the generic parts of the DMA descriptor */
        pcie_dma_setup(&dma_cmd,
                       __signal_number(&cmpl_sig), arg_trans_sz, arg_doff);

        /* Do work until done */
        for (;;) {
            trans = cls_test_sub(&num_dma_trans, 1);

            dma_addr_from_idx(trans, &addr_hi, &addr_lo, &unused);

            dma_cmd.pcie_addr_hi = addr_hi;
            dma_cmd.pcie_addr_lo = addr_lo;
            dma_cmd_wr = dma_cmd;

            /* Work out if we read or write. For Read/Write tests use
             * the transaction number: Uneven are reads, even are writes */
            if (test_no == BW_DMA_RD)
                read = 1;
            else if (test_no == BW_DMA_WR)
                read = 0;
            else if (trans & 1)
                read = 1;
            else
                read = 0;

            if (read)
                __pcie_dma_enq(0, &dma_cmd_wr, NFP_PCIE_DMA_FROMPCI_LO,
                               sig_done, &enq_sig);
            else
                __pcie_dma_enq(0, &dma_cmd_wr, NFP_PCIE_DMA_TOPCI_LO,
                               sig_done, &enq_sig);

            wait_for_all(&cmpl_sig, &enq_sig);

            /* Stop if this was the last transaction. */
            if (trans <= 1)
                break;
        }

        /* Context which processed the last DMA signals master. who is
         * ME 0 CTX 0 in the same island.  */
        if (trans == 1)
            signal_me(meid >> 4, 0, 0, PCIEBENCH_CTRL_SIGNO);
    }
}

/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*- */
