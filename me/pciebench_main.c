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

#include "shared.c"

int
main(void)
{
    __gpr struct test_params params;
    __gpr struct test_result result;

    __gpr uint32_t tmp;
    __lmem uint32_t *lm_tmp;
    __cls uint32_t *mem_tmp;

    __NFP_BUF_LOC volatile uint64_t *buf_tmp = nfp_buf;

    __gpr int res;
    __gpr int i;

    if (ctx() == 0) {
        /* Init the Pseudo Random number. Make sure we generate a
         * number every cycle. */
        tmp = local_csr_read(local_csr_ctx_enables);
        tmp |= 1 << 30;
        local_csr_write(local_csr_ctx_enables, tmp);
        local_csr_write(local_csr_pseudo_random_number, 0xdeadbeef);

        /* Initialise the NFP buffer with a known pattern. Useful for Debug. */
        for (i = 0; i < NFP_BUF_SZ/sizeof(uint64_t); i++, buf_tmp++)
            *buf_tmp = 0x0000beef0000b00f | ((uint64_t)i << 48) | (i << 16);

        /* Setup journals */
        MEM_JOURNAL_CONFIGURE(test_journal);
        MEM_JOURNAL_CONFIGURE(debug_journal);
    } else {
        /* Kill other contexts for now */
        dma_bw_worker();
    }

    /* Only context 0 executes the following code */
    for (;;) {
        if (test_ctrl <= 0)
            continue;

        /* Copy test configuration to local memory/registers */
        params = test_params;
        local_csr_write(local_csr_mailbox0, params.p0);
        local_csr_write(local_csr_mailbox1, params.p1);
        local_csr_write(local_csr_mailbox2, params.p2);
        local_csr_write(local_csr_mailbox3, params.p3);

        /* Copy the DMA addresses for each chunk to local memory */
        lm_tmp = (__lmem uint32_t *)chunk_dma_addrs;
        mem_tmp = (__cls uint32_t *)host_dma_addrs;
        for (i = 0; i < sizeof(chunk_dma_addrs); i += 4) {
            tmp = *mem_tmp++;
            *lm_tmp++ = tmp;
        }

        switch (test_ctrl) {
        case LAT_CMD_RD:
            res = cmd_lat(&params, &result, LAT_CMD_RD);
            break;

        case LAT_CMD_WRRD:
            res = cmd_lat(&params, &result, LAT_CMD_WRRD);
            break;

        case LAT_DMA_RD:
            res = dma_lat(&params, &result, LAT_DMA_RD);
            break;

        case LAT_DMA_WRRD:
            res = dma_lat(&params, &result, LAT_DMA_WRRD);
            break;

        case BW_DMA_RD:
        case BW_DMA_WR:
        case BW_DMA_RW:
            res = dma_bw(&params, &result, test_ctrl);
            break;

        default:
            res = -1;
            continue;
        }

        test_result = result;
        test_ctrl = res;
    }

    return 0;
}

/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*- */
