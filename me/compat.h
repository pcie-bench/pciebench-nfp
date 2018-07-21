/*
 * Copyright (C) 2015-2018 Rolf Neugebauer. All rights reserved.
 * Copyright (C) 2015 Netronome Systems, Inc. All rights reserved.
 *
 * This software may be redistributed under either of two provisions:
 *
 * 1. The GNU General Public License version 2 (see
 *    http://www.gnu.org/licenses/old-licenses/gpl-2.0.html or
 *    COPYING.txt file) when it is used for Linux or other
 *    compatible free software as defined by GNU at
 *    http://www.gnu.org/licenses/license-list.html.
 *
 * 2. Or under a non-free commercial license executed directly with
 *    Netronome. The direct Netronome license does not apply when the
 *    software is used as part of the Linux kernel.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _COMPAT_H_
#define _COMPAT_H_

/*
 * SDK 5.x (for the NFP-6000) and SDK 4.x (for the NFP-3200) provide
 * different basic type definitions and built-in functions. In this
 * file we hide the differences between them.  We basically define
 * similar types/functions for the SDK 4.x as defined by SDK 5.x.
 *
 * This list is by no means complete. We only define types/functions
 * used for the PCIe micro-benchmark code.
 */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_arr) (sizeof(_arr)/sizeof(*(_arr)))
#endif

/* Round up to the next 64B aligned address */
#ifndef roundup64
#define roundup64(_addr) (_addr + (64 - 1) & ~(64 - 1))
#endif


#ifndef __NFP_IS_3200
#include <nfp.h>
#else

/* Shorthand for various storage attributes */
#define __mem           __declspec(dram)
#define __emem          __declspec(dram)
#define __cls           __declspec(cls)
#define __lmem          __declspec(local_mem)
#define __export        __declspec(export)
#define __import        __declspec(import)
#define __shared        __declspec(shared)
#define __ptr40         __declspec(ptr40)
#define __xread         __declspec(read_reg)
#define __xwrite        __declspec(write_reg)
#define __xrw           __declspec(read_write_reg)
#define __gpr           __declspec(gp_reg)
#define __nnr           __declspec(nn_local_reg)
#define __visible       __declspec(visible)
#define __remote        __declspec(remote)
#define __align(x)      __declspec(aligned(x))
#define __packed        __declspec(packed)

/* ME related built-in functions */
int __LoadTimeConstant(char *name);

#define __MEID          __LoadTimeConstant("__UENGINE_ID")
#define ctx() __ctx()
unsigned int __ctx(void);

/* Signal definitions */
typedef int SIGNAL_MASK;
typedef __declspec(signal) int SIGNAL;
typedef __declspec(signal_pair) struct SIGNAL_PAIR {
    int even;
    int odd;
} SIGNAL_PAIR;
typedef enum {
    sig_done,
    ctx_swap,
    sig_none,
} sync_t;
typedef enum {
    kill,
    voluntary,
    bpt
} signal_t;
SIGNAL_MASK __signals(...);
int __signal_number(volatile void *sig, ...);
void __wait_for_all(...);
#define wait_for_all __wait_for_all
void __wait_for_any(...);
#define wait_for_any __wait_for_any

/* Other build-in functions */
int __is_ct_const(int v);
void __ct_assert(int v, char *reason);
void __implicit_read(void *sig_or_xfer, ...);
void __implicit_write(void *sig_or_xfer, ...);
void __assign_relative_register(void *sig_or_reg, int reg_num);
void __critical_path();

/* Local ME CSR definitions and functions */
enum local_csr_e {
    local_csr_ctx_enables                   = 0x006,
    local_csr_active_ctx_sts                = 0x011,
    local_csr_timestamp_low                 = 0x030,
    local_csr_timestamp_high                = 0x031,
    local_csr_next_neighbor_signal          = 0x040,
    local_csr_prev_neighbor_signal          = 0x041,
    local_csr_same_me_signal                = 0x042,
    local_csr_pseudo_random_number          = 0x052,
    local_csr_mailbox0                      = 0x05C,
    local_csr_mailbox1                      = 0x05D,
    local_csr_mailbox2                      = 0x05E,
    local_csr_mailbox3                      = 0x05F,
};
unsigned int local_csr_read(enum local_csr_e csr);
void local_csr_write(enum local_csr_e csr, unsigned int val);

/* Size specifications */
#define SZ_2            (1 <<  1)
#define SZ_4            (1 <<  2)
#define SZ_8            (1 <<  3)
#define SZ_16           (1 <<  4)
#define SZ_32           (1 <<  5)
#define SZ_64           (1 <<  6)
#define SZ_128          (1 <<  7)
#define SZ_256          (1 <<  8)
#define SZ_512          (1 <<  9)
#define SZ_1K           (1 << 10)
#define SZ_2K           (1 << 11)
#define SZ_4K           (1 << 12)
#define SZ_8K           (1 << 13)
#define SZ_16K          (1 << 14)
#define SZ_32K          (1 << 15)
#define SZ_64K          (1 << 16)
#define SZ_128K         (1 << 17)
#define SZ_256K         (1 << 18)
#define SZ_512K         (1 << 19)
#define SZ_1M           (1 << 20)
#define SZ_2M           (1 << 21)
#define SZ_4M           (1 << 22)
#define SZ_8M           (1 << 23)
#define SZ_16M          (1 << 24)
#define SZ_32M          (1 << 25)
#define SZ_64M          (1 << 26)
#define SZ_128M         (1 << 27)
#define SZ_256M         (1 << 28)
#define SZ_512M         (1 << 29)
#define SZ_1G           (1 << 30)
#define SZ_2G           (1 << 31)

#define __log2(val)          \
    ((val) >= SZ_2G   ? 31 : \
     (val) >= SZ_1G   ? 30 : \
     (val) >= SZ_512M ? 29 : \
     (val) >= SZ_256M ? 28 : \
     (val) >= SZ_128M ? 27 : \
     (val) >= SZ_64M  ? 26 : \
     (val) >= SZ_32M  ? 25 : \
     (val) >= SZ_16M  ? 24 : \
     (val) >= SZ_8M   ? 23 : \
     (val) >= SZ_4M   ? 22 : \
     (val) >= SZ_2M   ? 21 : \
     (val) >= SZ_1M   ? 20 : \
     (val) >= SZ_512K ? 19 : \
     (val) >= SZ_256K ? 18 : \
     (val) >= SZ_128K ? 17 : \
     (val) >= SZ_64K  ? 16 : \
     (val) >= SZ_32K  ? 15 : \
     (val) >= SZ_16K  ? 14 : \
     (val) >= SZ_8K   ? 13 : \
     (val) >= SZ_4K   ? 12 : \
     (val) >= SZ_2K   ? 11 : \
     (val) >= SZ_1K   ? 10 : \
     (val) >= 512     ?  9 : \
     (val) >= 256     ?  8 : \
     (val) >= 128     ?  7 : \
     (val) >= 64      ?  6 : \
     (val) >= 32      ?  5 : \
     (val) >= 16      ?  4 : \
     (val) >= 8       ?  3 : \
     (val) >= 4       ?  2 : \
     (val) >= 2       ?  1 : \
     (val) >= 1       ?  0 : -1)

#endif /* !NFP6000 */

#endif /* _COMPAT_H_ */
