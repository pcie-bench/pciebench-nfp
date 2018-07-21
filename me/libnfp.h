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

#ifndef _LIBNFP_H_
#define _LIBNFP_H_

/*
 * In order to avoid external dependencies on other packages and
 * repositories, here we replicate a small set of functions to
 * interact with the NFP hardware. Once these are more widely
 * available this file will be removed.
 *
 * We also hide differences between the NFP-3200 and the NFP-6000 in
 * these functions.
 *
 * Note: These functions are not general purpose.
 */


/*
 * ME level definitions and functions
 */
__intrinsic unsigned int __ME(void);
__intrinsic void ctx_wait(signal_t sig);
__intrinsic unsigned int ts_lo_read(void);
__intrinsic unsigned int ts_hi_read(void);
__intrinsic void signal_ctx(unsigned int ctx, unsigned int sig_no);
__intrinsic void signal_next_ctx(unsigned int sig_no);
__intrinsic void signal_next_me(unsigned int ctx, unsigned int sig_no);
__intrinsic void signal_me(unsigned int isl, unsigned int me,
                           unsigned int ctx, unsigned int sig_no);


/*
 * CLS functions
 */
__intrinsic unsigned int cls_test_sub(__cls void* add, unsigned int val);

/*
 * Memory unit macros and functions
 */
#ifdef __NFP_IS_3200
#define MEM_JOURNAL_DECLARE(_rnum, _name, _entries)                     \
    __export __mem __align(_entries * 4) uint32_t _name[_entries];      \
    const int _name##_ring_no = _rnum
#define MEM_JOURNAL_DECLARE_EXT(_name)          \
    extern const int _name##_ring_no;
#else
#define MEM_JOURNAL_DECLARE(_rnum, _name, _entries)                      \
    __asm {.alloc_mem _name emem0 global (_entries * 4) (_entries * 4)}; \
    __asm {.init_mu_ring _rnum _name};                                   \
    const int _name##_ring_no = _rnum
#define MEM_JOURNAL_DECLARE_EXT(_name)          \
    extern const int _name##_ring_no
#endif

#ifdef __NFP_IS_3200
#define MEM_JOURNAL_CONFIGURE(_name) \
    mem_journal_setup(_name##_ring_no, _name, sizeof(_name))
#else
#define MEM_JOURNAL_CONFIGURE(_name)
#endif

#ifdef __NFP_IS_3200
#define MEM_JOURNAL_FAST(_name, _val)                   \
    mem_ring_journal_fast(_name##_ring_no, _val)
#else
#define MEM_JOURNAL_FAST(_name, _val)                                   \
    do {                                                                \
        const int _name##_addr_hi = (__link_sym(#_name) >> 8) & 0xff000000; \
        mem_ring_journal_fast(_name##_ring_no, _name##_addr_hi, _val); \
    } while (0)
#endif

/**
 * Setup/Configure a memory journal
 */
#ifdef __NFP_IS_3200
__intrinsic void mem_journal_setup(unsigned int rnum,
                                   __mem void *base, size_t log2size);
#endif

/**
 * Fast journal an entry onto memory ring
 */
#ifdef __NFP_IS_3200
__intrinsic void mem_ring_journal_fast(unsigned int rnum, unsigned int value);
#else
__intrinsic void mem_ring_journal_fast(unsigned int rnum, unsigned int addr_hi,
                                       unsigned int value);
#endif

/*
 * PCIe functions
 */
/**
 * Reconfigure a CPP to PCIe BAR.
 * Island number ignored on 3200
 */
__intrinsic void pcie_c2p_barcfg(unsigned int pcie_isl, unsigned char bar_idx,
                                 unsigned int addr_hi, unsigned int addr_lo,
                                 unsigned char req_id);

/**
 * Read data from the host through a CPP2PCIe BAR
 * Island number ignored on 3200
 */
__intrinsic void __pcie_read(__xread void *data, unsigned int pcie_isl,
                             unsigned char bar_idx, unsigned int addr_hi,
                             unsigned int addr_lo, size_t size,
                             size_t max_size, sync_t sync, SIGNAL *sig);

__intrinsic void pcie_read(__xread void *data, unsigned int pcie_isl,
                           unsigned char bar_idx, unsigned int addr_hi,
                           unsigned int addr_lo, size_t size);

/**
 * Write data to the host through a CPP2PCIe BAR
 * Island number ignored on 3200
 */
__intrinsic void __pcie_write(__xwrite void *data, unsigned int pcie_isl,
                              unsigned char bar_idx, unsigned int addr_hi,
                              unsigned int addr_lo, size_t size,
                              size_t max_size, sync_t sync, SIGNAL *sig);

__intrinsic void pcie_write(__xwrite void *data, unsigned int pcie_isl,
                            unsigned char bar_idx, unsigned int addr_hi,
                            unsigned int addr_lo, size_t size);


/*
 * PCIe DMA descriptor
 */
#ifdef __NFP_IS_3200
#define NFP_PCIE_DMA_TOPCI_HI       0x40000
#define NFP_PCIE_DMA_TOPCI_LO       0x40010
#define NFP_PCIE_DMA_FROMPCI_HI     0x40020
#define NFP_PCIE_DMA_FROMPCI_LO     0x40030


/**
 * Structure of a DMA completion for signals
 */
union pcie_dma_completion {
    __packed struct {
        unsigned int pad1:16;
        unsigned int reserved1:1;
        unsigned int cl:4;
        unsigned int me:4;
        unsigned int ctx:3;
        unsigned int signo:4;
    };
    __packed struct {
        unsigned int pad2:16;
        unsigned int generate_event:1;
        unsigned int event_data:11;
        unsigned int event_type:4;
    };
    __packed struct {
        unsigned int pad3:16;
        unsigned int completion:16;
    };
};

/**
 * Structure of a DMA command
 */
struct nfp_pcie_dma_cmd {
    union {
        struct {
            unsigned int cpp_addr_lo;

            unsigned int completion:16;
            unsigned int token:2;
            unsigned int word_only:1;
            unsigned int cpp_target64:1;
            unsigned int cpp_target:4;
            unsigned int cpp_addr_hi:8;

            unsigned int pcie_addr_lo;

            unsigned int len:12;
            unsigned int req_id:8;
            unsigned int override_rid:1;
            unsigned int no_snoop:1;
            unsigned int relaxed:1;
            unsigned int reserved:1;
            unsigned int pcie_addr_hi:8;
        };
        unsigned int __raw[4];
    };
};
#else
#define NFP_PCIE_DMA_TOPCI_HI                              0x40000
#define NFP_PCIE_DMA_TOPCI_MED                             0x40020
#define NFP_PCIE_DMA_TOPCI_LO                              0x40040
#define NFP_PCIE_DMA_FROMPCI_HI                            0x40060
#define NFP_PCIE_DMA_FROMPCI_MED                           0x40080
#define NFP_PCIE_DMA_FROMPCI_LO                            0x400a0

/**
 * Structure of a DMADescrConfig register
 */
struct nfp_pcie_dma_cfg {
    union {
        struct {
            unsigned int __reserved_29:3;
            unsigned int signal_only_odd:1;
            unsigned int end_pad_odd:2;
            unsigned int start_pad_odd:2;
            unsigned int id_based_order_odd:1;
            unsigned int relaxed_order_odd:1;
            unsigned int no_snoop_odd:1;
            unsigned int target_64_odd:1;
            unsigned int cpp_target_odd:4;
            unsigned int __reserved_13:3;
            unsigned int signal_only_even:1;
            unsigned int end_pad_even:2;
            unsigned int start_pad_even:2;
            unsigned int id_based_order_even:1;
            unsigned int relaxed_order_even:1;
            unsigned int no_snoop_even:1;
            unsigned int target_64_even:1;
            unsigned int cpp_target_even:4;
        };
        unsigned int __raw;
    };
};

/**
 * Structure of a DMA command
 */
#define     NFP_PCIE_DMA_CMD_DMA_MODE_shf                    14
struct nfp_pcie_dma_cmd {
    union {
        struct {
            unsigned int cpp_addr_lo:32;

            unsigned int mode_sel:2;
            unsigned int dma_mode:16;
            unsigned int cpp_token:2;
            unsigned int dma_cfg_index:4;
            unsigned int cpp_addr_hi:8;

            unsigned int pcie_addr_lo:32;

            unsigned int length:12;
            unsigned int rid:8;
            unsigned int rid_override:1;
            unsigned int trans_class:3;
            unsigned int pcie_addr_hi:8;
        };
        unsigned int __raw[4];
    };
};
__intrinsic void __pcie_dma_cfg_set_pair(unsigned int pcie_isl,
                                         unsigned int index, __xwrite struct
                                         nfp_pcie_dma_cfg *new_cfg,
                                         sync_t sync, SIGNAL *sig);

__intrinsic void pcie_dma_cfg_set_pair(unsigned int pcie_isl,
                                       unsigned int index, __xwrite struct
                                       nfp_pcie_dma_cfg *new_cfg);

#endif
/**
 * Enqueue a DMA descriptor
 * @param pcie_isl          PCIe island (0-3) to address
 * @param cmd               DMA command to send
 * @param queue             queue to use, e.g. NFP_PCIE_DMA_TOPCI_HI
 */
__intrinsic void __pcie_dma_enq(unsigned int pcie_isl,
                                __xwrite struct nfp_pcie_dma_cmd *cmd,
                                unsigned int queue, sync_t sync, SIGNAL *sig);


#endif /* _LIBNFP_H_ */
