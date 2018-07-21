# Caller must provide location of SDK5 and/or SDK4 toolchain install dir
sdk_defined =
ifdef SDK5
	sdk_defined = yes
endif
ifdef SDK4
	sdk_defined = yes
endif
ifndef sdk_defined
ifneq ($(MAKECMDGOALS),clean)
$(error You must specify SDK5 and/or SDK4)
endif
endif

sdk5_NFCC := $(SDK5)/bin/nfcc
sdk5_NFAS := $(SDK5)/bin/nfas
sdk5_NFLD := $(SDK5)/bin/nfld
sdk5_STDLIB := $(SDK5)/components/standardlibrary/microc
sdk5_SRCS :=  $(sdk5_STDLIB)/src/rtl.c

sdk4_NFCC := $(SDK4)/bin/nfcc
sdk4_NFAS := $(SDK4)/bin/nfas
sdk4_NFLD := $(SDK4)/bin/nfld
sdk4_STDLIB := $(SDK4)/components/standardlibrary/microc

DEPS := $(wildcard *.c) $(wildcard *.h) Makefile

MAIN_SRCS := pciebench_main.c pcie_cmd.c pcie_dma.c utils.c libnfp.c
WORKERS_SRCS := dma_worker_main.c pcie_dma.c utils.c libnfp.c

CFGLAGS_COMMON := -W3 -Ob2 -Qspill=7 -Qnctx_mode=8 \
		  -Qno_decl_volatile -single_dram_signal

nfp6000_CFLAGS := $(CFGLAGS_COMMON) \
		  -chip nfp-4xxx-b0 -mIPOPT_expose_intrinsics \
		  -I$(sdk5_STDLIB)/include

nfp3200_CFLAGS := $(CFGLAGS_COMMON) \
		  -I$(sdk4_STDLIB)/include


LDFLAGS := -rtsyms -mip

nfp6000_LDFLAGS := $(LDFLAGS) -chip nfp-4xxx-b0

nfp3200_LDFLAGS := $(LDFLAGS) -dr 0x00008000:0x7fff8000


ifdef SDK5
all: nfp6000_pciebench.nffw
endif

ifdef SDK4
all: nfp3200_pciebench.nffw
endif

clean:
	rm -f *.list *.obj *.uci *.ucp

nfp6000_pciebench_main.list: $(DEPS)
	 $(sdk5_NFCC) -Fe$(basename $@) $(nfp6000_CFLAGS) \
		$(MAIN_SRCS) $(sdk5_SRCS)
nfp6000_dma_worker_main.list: $(DEPS)
	 $(sdk5_NFCC) -Fe$(basename $@) $(nfp6000_CFLAGS) \
		$(WORKERS_SRCS) $(sdk5_SRCS)
nfp6000_nffw_deps := nfp6000_pciebench_main.list nfp6000_dma_worker_main.list
nfp6000_pciebench.nffw: $(nfp6000_nffw_deps)
	$(sdk5_NFLD) -elf $@ $(nfp6000_LDFLAGS) \
		     -u mei0.me0 -l nfp6000_pciebench_main.list \
		     -u mei0.me1 -l nfp6000_dma_worker_main.list \
		     -u mei0.me2 -l nfp6000_dma_worker_main.list \
		     -u mei0.me3 -l nfp6000_dma_worker_main.list \
		     -u mei0.me4 -l nfp6000_dma_worker_main.list \
		     -u mei0.me5 -l nfp6000_dma_worker_main.list \
		     -u mei0.me6 -l nfp6000_dma_worker_main.list \
		     -u mei0.me7 -l nfp6000_dma_worker_main.list \
		     -u mei0.me8 -l nfp6000_dma_worker_main.list \
		     -u mei0.me9 -l nfp6000_dma_worker_main.list \
		     -u mei0.me10 -l nfp6000_dma_worker_main.list \
		     -u mei0.me11 -l nfp6000_dma_worker_main.list

nfp3200_init.list: nfp3200_init.uc
	$(sdk4_NFAS) -t -W3 -R -lm 0 -nfp3240 -o $@ nfp3200_init.uc

nfp3200_pciebench_main.list: $(DEPS)
	$(sdk4_NFCC) -Fe$(basename $@) $(nfp3200_CFLAGS) \
		$(MAIN_SRCS) libnfcc.c
nfp3200_dma_worker_main.list: $(DEPS)
	$(sdk4_NFCC) -Fe$(basename $@) $(nfp3200_CFLAGS) \
		$(WORKERS_SRCS) libnfcc.c
nfp3200_nffw_deps := nfp3200_pciebench_main.list \
		     nfp3200_dma_worker_main.list \
		     nfp3200_init.list
nfp3200_pciebench.nffw: $(nfp3200_nffw_deps)
	$(sdk4_NFLD) -elf $@ $(nfp3200_LDFLAGS) \
		     -u cl0.me0 -l nfp3200_init.list \
		     -u cl1.me0 -l nfp3200_pciebench_main.list \
		     -u cl1.me1 -l nfp3200_dma_worker_main.list \
		     -u cl1.me2 -l nfp3200_dma_worker_main.list \
		     -u cl1.me3 -l nfp3200_dma_worker_main.list \
		     -u cl1.me4 -l nfp3200_dma_worker_main.list \
		     -u cl1.me5 -l nfp3200_dma_worker_main.list \
		     -u cl1.me6 -l nfp3200_dma_worker_main.list \
		     -u cl1.me7 -l nfp3200_dma_worker_main.list

