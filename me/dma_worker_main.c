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

/*
 * Main entry point for code running on the worker MEs for DMA BW tests
 */
#include <stdint.h>

#include "compat.h"
#include "libnfp.h"
#include "pciebench.h"

#include "shared.c"

int
main(void)
{
    /* Just call the main worker function. It does the rest. */
    dma_bw_worker();
    /* NOTREACHED */
}

/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*- */


