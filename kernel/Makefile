#
# This is a simple makefile wrapper around the Linux Kbuild
# infrastrcuture.  By default the kernel modules for the currently
# installed Linux kernel will be installed.  To compile for an
# alternative kernel, or if the kernel build directory is in a
# non-default location you can use the following to compile:
#
#    make [KDIR=/location/of/kernel/build]
#
# You must specify NFP_DIR to point to the base of the NFP base driver
#
# Supported make targets:
#
#    modules		build kernel modules
#    modules_install	build and install kernel modules
#    clean		clean up build directory
#
Q ?= @

KDIR ?= /lib/modules/`uname -r`/build

ifndef NFP_DIR
$(error NFP_DIR must be set to the location of the compiled NFP driver)
endif

NFP_DIR_ABS := $(realpath $(NFP_DIR))

nfp-kmod-abs-srcs := \
  $(wildcard $(addprefix $(NFP_DIR_ABS)/src/nfpcore/,*.c *.h))\
  $(wildcard $(addprefix $(NFP_DIR_ABS)/src/nfpcore/nfp3200/,*.h))\
  $(wildcard $(addprefix $(NFP_DIR_ABS)/src/nfpcore/nfp6000/,*.h))

nfp-kmod-rel-srcs := $(subst $(NFP_DIR_ABS)/src/,,$(nfp-kmod-abs-srcs))

modules: $(nfp-kmod-rel-srcs)
	$(MAKE) -C $(KDIR) M=$$PWD  $@

modules_install: $(nfp-kmod-rel-srcs)
	$(MAKE) -C $(KDIR) M=$$PWD  $@

clean: $(nfp-kmod-rel-srcs)
	$(MAKE) -C $(KDIR) M=$$PWD  $@
	$(Q) rm -rf nfpcore

$(nfp-kmod-rel-srcs):
	$(Q) mkdir -p $(@D)
	$(Q) cd $(@D) && ln -s -f $(NFP_DIR_ABS)/src/$@ .
