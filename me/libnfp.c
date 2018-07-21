/*
 * Copyright (C) 2015-2018 Rolf Neugebauer. All rights reserved.
 * Copyright (C) 2015 Netronome Systems, Inc. All rights reserved.
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

#include "compat.h"
#include "libnfp.h"

__intrinsic unsigned int
__ME(void)
{
    unsigned int ctxsts, menum, islclnum;

    ctxsts = local_csr_read(local_csr_active_ctx_sts);
    menum = ((ctxsts >> 3) - 4) & 0x7;
#ifdef __NFP_IS_3200
    islclnum = (ctxsts >> 25) & 0xf;
#else
    islclnum = (ctxsts >> 25) & 0x3f;
#endif
    return (islclnum << 4) + menum;
}

__intrinsic void
ctx_wait(signal_t sig)
{
    if (sig == kill)
        __asm ctx_arb[kill];
    else if (sig == voluntary)
        __asm ctx_arb[voluntary];
    else
        __asm ctx_arb[bpt];
}

__intrinsic unsigned int
ts_lo_read(void)
{
    return local_csr_read(local_csr_timestamp_low);
}

__intrinsic unsigned int
ts_hi_read(void)
{
    return local_csr_read(local_csr_timestamp_high);
}

#ifdef __NFP_IS_3200
__intrinsic unsigned int
local_csr_read(int mecsr)
{
    unsigned int result;

    __asm {
        local_csr_rd[__ct_const_val(mecsr)];
        immed[result, 0];
    }

    return result;
}

__intrinsic void
local_csr_write(int mecsr, unsigned int data)
{
    __asm local_csr_wr[__ct_const_val(mecsr), data];
}
#endif

#define NFP_MECSR_SAME_ME_SIGNAL_NEXT_CTX                  (1 << 7)
#define NFP_MECSR_SAME_ME_SIGNAL_CTX(x)                    (((x) & 7) << 0)
#define NFP_MECSR_SAME_ME_SIGNAL_SIG_NO(x)                 (((x) & 0xf) << 3)
#define NFP_MECSR_NEXT_NEIGHBOR_SIGNAL_SIG_NO(x)           (((x) & 0xf) << 3)
#define NFP_MECSR_NEXT_NEIGHBOR_SIGNAL_CTX(x)              (((x) & 7) << 0)

__intrinsic void
signal_ctx(unsigned int ctx, unsigned int sig_no)
{
    local_csr_write(local_csr_same_me_signal,
                    (NFP_MECSR_SAME_ME_SIGNAL_SIG_NO(sig_no) |
                     NFP_MECSR_SAME_ME_SIGNAL_CTX(ctx)));
}

__intrinsic void
signal_next_ctx(unsigned int sig_no)
{
    local_csr_write(local_csr_same_me_signal,
                    (NFP_MECSR_SAME_ME_SIGNAL_NEXT_CTX |
                     NFP_MECSR_SAME_ME_SIGNAL_SIG_NO(sig_no)));
}

__intrinsic void
signal_next_me(unsigned int ctx, unsigned int sig_no)
{
    local_csr_write(local_csr_next_neighbor_signal,
                    (NFP_MECSR_NEXT_NEIGHBOR_SIGNAL_SIG_NO(sig_no) |
                     NFP_MECSR_NEXT_NEIGHBOR_SIGNAL_CTX(ctx)));
}

__intrinsic void
signal_me(unsigned int islcl, unsigned int me,
          unsigned int ctx, unsigned int sig_no)
{
    unsigned int addr;
#ifdef __NFP_IS_3200
    unsigned int val;

    addr = 0x401c;
    val = (islcl & 0xf) << 11 | (me & 0xf) << 7 |
        (ctx & 0x7) << 4 | (sig_no & 0xf);
    __asm alu[--, --, B, val];
    __asm cap[fast_wr, ALU, interthread_sig]

#else
    addr = ((islcl & 0x3f) << 24 | ((me & 0xf) + 4) << 9 |
        (ctx & 0x7) << 6 | (sig_no & 0xf) << 2);
    __asm ct[interthread_signal, --, addr, 0, --];
#endif
}

/*
 * Stub library for access to *very* selected NFP features.  Note, we
 * do not perform any sanity checks here on arguments.
 */
#ifdef __NFP_IS_3200
typedef union {
    struct {
        unsigned int __two:4;
        unsigned int __rsvd:23;
        unsigned int ref_count:5;
    } ;
    unsigned int value;
} ind_override_cnt_t;
#else
struct nfp_mecsr_prev_alu {
    union {
        struct {
            unsigned int data16:16;
            unsigned int res:1;
            unsigned int ov_sig_ctx:1;
            unsigned int ov_sig_num:1;
            unsigned int length:5;
            unsigned int ov_len:1;
            unsigned int ov_bm_csr:1;
            unsigned int ove_data:3;
            unsigned int ove_master:2;
            unsigned int ov_sm:1;
        };
        unsigned int __raw;
    };
};
#endif


/*
 * CLS
 */
__intrinsic unsigned int
cls_test_sub(__cls void* addr, unsigned int val)
{
    __xrw unsigned int tmp;
    SIGNAL sig;

    tmp = val;
#ifdef __NFP_IS_3200
    __asm cls[test_and_sub_sat, tmp, addr, 0, 1], ctx_swap[sig];
#else
    __asm cls[test_subsat, tmp, addr, 0, 1], ctx_swap[sig];
#endif
    return tmp;
}


/*
 * Memory unit
 */
#ifdef __NFP_IS_3200
#define NFP_MEMRING_T2_EOP              (0x1 << 1)
#define NFP_MEMRING_T2_EOP_bf           0, 1, 1
#define NFP_MEMRING_T2_EOP_bit          1
#define NFP_MEMRING_T2_HEAD_PTR(x)      (((x) & 0xffffff) << 2)
#define NFP_MEMRING_T2_HEAD_PTR_bf      0, 25, 2
#define NFP_MEMRING_T2_RING_SIZE(x)     (((x) & 0xf) << 28)
#define NFP_MEMRING_T2_RING_SIZE_bf     0, 31, 26
#define NFP_MEMRING_T2_TYPE(x)          ((x) & 0x3)
#define NFP_MEMRING_T2_TYPE_bf          1, 1, 0
#define NFP_MEMRING_T2_TAIL_PTR(x)      (((x) & 0x3fffffff) << 2)
#define NFP_MEMRING_T2_TAIL_PTR_bf      1, 31, 2
#define NFP_MEMRING_T2_Q_COUNT(x)       ((x) & 0xffffff)
#define NFP_MEMRING_T2_Q_COUNT_of(x)    ((x) & 0xffffff)
#define NFP_MEMRING_T2_Q_COUNT_bf       2, 23, 0
#define NFP_MEMRING_T2_Q_PAGE(x)        (((x) & 0x3) << 24)
#define NFP_MEMRING_T2_Q_PAGE_bf        2, 25, 24
#define NFP_MEMRING_T2_Q_LOC(x)         (((x) & 0x3) << 30)
#define NFP_MEMRING_T2_Q_LOC_bf         2, 31, 30

__intrinsic void
mem_journal_setup(unsigned int rnum, __mem void *base, size_t size)
{
    __xwrite unsigned int desc[4];
    unsigned int entries = size / 4;
    SIGNAL sig;


    desc[0] = (NFP_MEMRING_T2_RING_SIZE(__log2(entries) - 9) |
               NFP_MEMRING_T2_HEAD_PTR(((unsigned) base) >> 2));

    desc[1] = (NFP_MEMRING_T2_TAIL_PTR(((unsigned) base) >> 2) |
               NFP_MEMRING_T2_TYPE(2));
    desc[2] = (NFP_MEMRING_T2_Q_LOC(0) |
               NFP_MEMRING_T2_Q_PAGE(((unsigned long long) base) >> 32) |
               NFP_MEMRING_T2_Q_COUNT(0));
    desc[3] = 0;

    __asm mem[write, desc, base, 0, 2], ctx_swap[sig];

    /* Now read the descriptor into queue array */
    __asm {
        alu[--, --, B, rnum, <<5];
        mem[rd_qdesc, --, 0, base], indirect_ref;
    }
}

__intrinsic void
mem_ring_journal_fast(unsigned int rnum, unsigned int value)
{
    __asm { alu[--, --, B, rnum, <<5], no_cc }
    __asm { mem[fast_journal, --, value, 0], indirect_ref }
}
#else /* NFP-6000 */
__intrinsic void
mem_ring_journal_fast(unsigned int rnum, unsigned int addr_hi,
                      unsigned int value)
{
    struct nfp_mecsr_prev_alu ind;

    ind.__raw = 0;
    ind.data16 = rnum;
    ind.ove_data = 1;
    __asm {
        alu[--, --, B, ind.__raw];
        mem[fast_journal,--, addr_hi, <<8, value, 0], indirect_ref
    }
}
#endif /* __NFP_IS_3200 */


/*
 * PCIe functions
 */

#ifdef __NFP_IS_3200
#define NFP_PCIE_BARCFG_C2P(_bar) (0x30020 + (0x4 * ((_bar) & 0x7)))
#define   NFP_PCIE_BARCFG_C2P_ARI_ENABLE   (0x1 << 28)
#define   NFP_PCIE_BARCFG_C2P_ARI(_x)      (((_x) & 0xff) << 20)
#define   NFP_PCIE_BARCFG_C2P_ADDR_msk     (0x7ffff)
#else
#define NFP_PCIE_BARCFG_C2P(_bar) (0x30180 + ((_bar) * 0x4))
#define   NFP_PCIE_BARCFG_C2P_ARI_ENABLE   (1 << 29)
#define   NFP_PCIE_BARCFG_C2P_ARI(x)       (((x) & 0xff) << 21)
#define   NFP_PCIE_BARCFG_C2P_ADDR_msk     (0x1fffff)
#endif

__intrinsic
void pcie_c2p_barcfg(unsigned int pcie_isl, unsigned char bar_idx,
                     unsigned int addr_hi, unsigned int addr_lo,
                     unsigned char req_id)
{
    unsigned int isl, bar_addr, tmp;
    __xwrite unsigned int wr_val;
    __xread unsigned int rd_val;
    SIGNAL wr_sig, rd_sig;

    isl = pcie_isl << 30;
    bar_addr = NFP_PCIE_BARCFG_C2P(bar_idx);

#ifdef __NFP_IS_3200
    __asm dbl_shf[tmp, addr_hi, addr_lo, >>29];
#else
    tmp = addr_hi >> 3;
#endif

    tmp &= NFP_PCIE_BARCFG_C2P_ADDR_msk;

    /* Configure RID if req_id is non-zero or not constant */
    if ((!__is_ct_const(req_id)) || (req_id != 0)) {
        tmp |= NFP_PCIE_BARCFG_C2P_ARI_ENABLE;
        tmp |= NFP_PCIE_BARCFG_C2P_ARI(req_id);
    }

    wr_val = tmp;

#ifdef __NFP_IS_3200
    __asm pcie[write_pci, wr_val, bar_addr, 0, 1], sig_done[wr_sig];
    __asm pcie[read_pci,  rd_val, bar_addr, 0, 1], sig_done[rd_sig];
#else
    __asm pcie[write_pci, wr_val, isl, <<8, bar_addr, 1], sig_done[wr_sig];
    __asm pcie[read_pci,  rd_val, isl, <<8, bar_addr, 1], sig_done[rd_sig];
#endif
    wait_for_all(&wr_sig, &rd_sig);
}

/* Only support PCIe reads/writes with run-time defined values. More
 * efficient versions exist for compile time constants. */
#ifdef __NFP_IS_3200
#define _PCIE_C2P_CMD(cmdname, data, isl, bar, addr_hi, addr_lo,        \
                      size, max_size, sync, sig)                        \
do {                                                                    \
    unsigned int count = (size >> 2);                                   \
    ind_override_cnt_t ind;                                             \
    unsigned int addr;                                                  \
                                                                        \
    addr = (addr_lo & 0x1fffffff) | (bar_idx << 29);                    \
                                                                        \
    ind.value = 0;                                                      \
    ind.__two = 2;                                                      \
    ind.ref_count = count - 1;                                          \
                                                                        \
    if (sync == sig_done) {                                             \
        __asm alu[--, --, B, ind]                                       \
        __asm pcie[cmdname, *data, addr, 0, __ct_const_val(count)],     \
            sig_done[*sig], indirect_ref                                \
    } else {                                                            \
        __asm alu[--, --, B, ind]                                       \
        __asm pcie[cmdname, *data, addr, 0, __ct_const_val(count)],     \
             ctx_swap[*sig], indirect_ref                               \
    }                                                                   \
} while (0)
#else
#define _PCIE_C2P_CMD(cmdname, data, isl, bar, addr_hi, addr_lo,        \
                      size, max_size, sync, sig)                        \
do {                                                                    \
    struct nfp_mecsr_prev_alu ind;                                      \
    unsigned int count = (size >> 2);                                   \
    unsigned int max_count = (max_size >> 2);                           \
    unsigned int addr;                                                  \
                                                                        \
    addr = (isl << 30) | ((bar & 0x7) << 27) | ((addr_hi & 0x7) << 24); \
                                                                        \
    ind.__raw = 0;                                                      \
    ind.ov_len = 1;                                                     \
    ind.length = count - 1;                                             \
                                                                        \
    if (sync == sig_done) {                                             \
        __asm { alu[--, --, B, ind.__raw] }                             \
        __asm { pcie[cmdname, *data, addr, <<8, addr_lo,                \
                     __ct_const_val(max_count)], sig_done[*sig],        \
                indirect_ref }                                          \
    } else {                                                            \
        __asm { alu[--, --, B, ind.__raw] }                             \
        __asm { pcie[cmdname, *data, addr_hi, <<8, addr_lo,             \
                     __ct_const_val(max_count)], ctx_swap[*sig],        \
                     indirect_ref }                                     \
    }                                                                   \
} while (0)
#endif


__intrinsic void
__pcie_read(__xread void *data, unsigned int pcie_isl, unsigned char bar_idx,
            unsigned int addr_hi, unsigned int addr_lo,
            size_t size, size_t max_size, sync_t sync, SIGNAL *sig)
{
    _PCIE_C2P_CMD(read, data, pcie_isl, bar_idx, addr_hi, addr_lo, size, \
                  max_size, sync, sig);
}

__intrinsic void
__pcie_write(__xwrite void *data, unsigned int pcie_isl, unsigned char bar_idx,
             unsigned int addr_hi, unsigned int addr_lo, size_t size,
             size_t max_size, sync_t sync, SIGNAL *sig)
{
    _PCIE_C2P_CMD(write, data, pcie_isl, bar_idx, addr_hi, addr_lo, size, \
                  max_size, sync, sig);
}


#ifdef __NFP_IS_3200
__intrinsic void
__pcie_dma_enq(unsigned int pcie_isl, __xwrite struct nfp_pcie_dma_cmd *cmd,
               unsigned int queue, sync_t sync, SIGNAL *sig)
{
    unsigned int count = (sizeof(struct nfp_pcie_dma_cmd) >> 2);

    if (sync == ctx_swap)
        __asm pcie[write_pci, *cmd, queue, 0, __ct_const_val(count)],   \
            ctx_swap[*sig];
    else
        __asm pcie[write_pci, *cmd, queue, 0, __ct_const_val(count)],   \
            sig_done[*sig];
}

#else
#define NFP_PCIE_DMA_CFG0                                  0x400c0

__intrinsic void
__pcie_dma_cfg_set_pair(unsigned int pcie_isl, unsigned int index,
                        __xwrite struct nfp_pcie_dma_cfg *new_cfg,
                        sync_t sync, SIGNAL *sig)
{
    unsigned int count;
    unsigned int reg_no;
    unsigned int addr_lo;
    __gpr unsigned int addr_hi;

    count = (sizeof(struct nfp_pcie_dma_cfg) >> 2);
    reg_no = (((index >> 1) & 0x7) << 2);
    addr_lo = NFP_PCIE_DMA_CFG0 + reg_no;
    addr_hi = pcie_isl << 30;

    if (sync == ctx_swap)
        __asm pcie[write_pci, *new_cfg, addr_hi, <<8, addr_lo, \
                   __ct_const_val(count)], ctx_swap[*sig];
    else
        __asm pcie[write_pci, *new_cfg, addr_hi, <<8, addr_lo, \
                   __ct_const_val(count)], sig_done[*sig];
}

__intrinsic void
pcie_dma_cfg_set_pair(unsigned int pcie_isl, unsigned int index,
                      __xwrite struct nfp_pcie_dma_cfg *new_cfg)
{
    SIGNAL sig;

    __pcie_dma_cfg_set_pair(pcie_isl, index, new_cfg, ctx_swap, &sig);
}

__intrinsic void
__pcie_dma_enq(unsigned int pcie_isl, __xwrite struct nfp_pcie_dma_cmd *cmd,
               unsigned int queue, sync_t sync, SIGNAL *sig)
{
    unsigned int count = (sizeof(struct nfp_pcie_dma_cmd) >> 2);
    unsigned int addr_hi = pcie_isl << 30;

    if (sync == ctx_swap)
        __asm pcie[write_pci, *cmd, addr_hi, <<8, queue, \
                   __ct_const_val(count)], ctx_swap[*sig];
    else
        __asm pcie[write_pci, *cmd, addr_hi, <<8, queue, \
                   __ct_const_val(count)], sig_done[*sig];
}

#endif
