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


/**
 * Reserve some memory regions used by the linker/driver
 */
.global_mem __page_zero          dram+0x00000 0x1000 reserved
.global_mem __nfp_nffw           dram+0x01000 0x1000 reserved

/**
 * Initialise all timestamp counters to a single value at once
 *
 * GLOBAL_TIMESTAMP_EN = bit 7
 *  GLOBAL_TIMESTAMP_RESTART = bit 6
 *
 * Controls all MicroEngines, PCIe, ARM-gasket, MSF clusters and
 * SHaC Cluster Timestamps, so that all of the Timestamps can
 * be programmed with the same value and enabled at once.
 *
 * To ensure proper resetting of all Global Timestamps use the
 * following register write sequence:
 *
 * STEP 1:    ~U Write GLOBAL_TIMESTAMP_RESTART to '1'
 *            while GLOBAL_TIMESTAMP_EN is '0', then
 *
 * STEP 2:    ~U Write GLOBAL_TIMESTAMP_RESTART to '0'
 *            while GLOBAL_TIMESTAMP_EN is '0', then
 *
 * STEP 3:    ~U Write GLOBAL_TIMESTAMP_RESTART to '0'
 *             while GLOBAL_TIMESTAMP_EN is '1'
 */
#macro timestamp_init()
.begin
    .reg $control
    .sig cap_sig
    .sig misc_sig
    .reg base

    // STEP 1
    cap[read,$control,MISC_CONTROL],sig_done[cap_sig]
cap_sig#:
    br_!signal[cap_sig, cap_sig#]

    .if ( ($control & 0x80) )
        immed[base, 0x80]
        alu[--,$control,AND~,base] // Write GLOBAL_TIMESTAMP_EN to '0'
        cap[fast_wr,ALU,MISC_CONTROL]
    .endif

    cap[read,$control,MISC_CONTROL],sig_done[cap_sig]
cap_sig_step_1#:
    br_!signal[cap_sig, cap_sig_step_1#]
    alu[$control, $control, OR, 1, <<6] // Write GLOBAL_TIMESTAMP_RESTART to '1'
    cap[fast_wr,ALU,MISC_CONTROL]

    // STEP 2
    cap[read,$control,MISC_CONTROL],sig_done[cap_sig]
cap_sig_step_2#:
    br_!signal[cap_sig, cap_sig_step_2#]
    immed[base, 0x40]
    alu[--,$control,AND~,base] // Write GLOBAL_TIMESTAMP_RESTART to '0'
    cap[fast_wr,ALU,MISC_CONTROL]

    // STEP 3
    cap[read,$control,MISC_CONTROL],sig_done[cap_sig]
cap_sig_step_3_a#:
    br_!signal[cap_sig, cap_sig_step_3_a#]
    alu[--,$control,OR, 1, <<7] // Write GLOBAL_TIMESTAMP_EN to '1'
    cap[fast_wr,ALU,MISC_CONTROL]
    cap[read,$control,MISC_CONTROL],sig_done[cap_sig]

cap_sig_step_3_b#:
    br_!signal[cap_sig, cap_sig_step_3_b#]
    immed[base, 0x40]
    alu[--,$control,AND~,base] // Write GLOBAL_TIMESTAMP_RESTART to '0'
    cap[fast_wr,ALU,MISC_CONTROL]
.end
#endm // timestamp_init


.begin
    .if (ctx() == 0)
        timestamp_init()
    .endif
    ctx_arb[voluntary]
.end
