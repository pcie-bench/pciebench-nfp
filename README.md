# pcie-bench for Netronome's NFP cards

**Note: I don't have access to a NFP system anymore, so I'm unable to
test this code.**

This repository contains a set of PCIe Microbenchmarks written for
Netronome's NFP-3200 and NFP-6000 based cards.

The code for the benchmarks is located in the `./me` directory.  A
kernel module, used for allocating and mapping memory to be used by
the tests is located in the `./kernel` directory.  The benchmarks are
controlled by a python program located in the `./python` directory.
The python control program requires a helper program located in the
`./user` directory.  Finally, data from the tests run an several
different platforms is located in the `./data` directory.


## Pre-requisite

In order to compile and run the tests you must have a Linux system
with python installed (Python 2.7 or Python 3.x).  You also need the
necessary packages installed to compile Linux kernel modules for the
currently running kernel.

You also need to have the source code for the NFP drivers located
somewhere. The kernel code was written against an older version of NFP
driver and currently also does not work on newer kernel. The latest
I've complied the driver against was 4.4.x.

```sh
git clone https://github.com/Netronome/nfp-drv-kmods.git
cd nfp-drv-kmods.git
git checkout c7305d5828e9bc529e6bfb06e49822ee8f27ebdb
```

Finally, you also need to have the NFP userspace utilities and
libraries installed.  The utilities are assumed to be in your path.

The repository contains pre-compiled binary firmware images for the
NFP-3200 and the NFP-6000. If you want to change/recompile the NFP
firmware you also need the NFP SDK version 4.7 for the NFP-3200 and/or
version 5.x for the NFP-6xxx installed.

In addition, it is useful (but not necessary) to have the following
packages installed (for Ubuntu 14.04):

```sh
sudo apt-get install hwloc-nox
sudo apt-get install numactl
```

## Compiling

### Compile the Linux kernel module

Assuming the NFP driver code was clone into the top directory of this
repository:

```sh
cd kernel
make NFP_DIR=../nfp-drv-kmods
```

If the NFP driver code is located elsewhere, adjust NFP_DIR
accordingly.


### Compile the helper utility

```sh
cd user
make
```

You may have do adjust the Makefile if the NFP libraries are not
located in a standard location.


### Compiling the FW (optional)

To re-compile the FW you need to specify the SDK4 and/or the SDK5
variable to compile for the NFP-3200 or the NFP-6000 respectively.

```sh
cd me
make SDK4=<path to NFP SDK 4.7> SDK5=<path to NFP SDK 5.x>
```


## Running the test suite

**To obtain accurate results it is important to run the tests on a
  idle system with minimal PCIe and CPU activity.  We strongly
  recommend running the tests in single user mode from the (serial)
  console.**

For all tests, the `nfp_pciebench.ko` kernel module must be loaded:

```sh
insmod ./kernel/nfp_pciebench.ko
```

You may have to unload the default `nfp.ko` or `nfp_net.ko` kernel
modules before.

The test execution is driven by the `nfp-pciebench.py` utility located
in the `./python` directory.  By default it runs the full suite, which
may take several hours to complete.

At a minimum you need to specify the location of the NFP firmware file
to use, the location of the helper utility and a name for the output
directory.  For example:

```sh
cd python
./nfp_pciebench.py -f ../me/nfp6000_pciebench.nffw \
    -u ../user/nfp-pciebench-helper -o foo
```

This collects general information about the system before running a
sequence of tests.  The data collected from the tests are printed out
on the console as well as written to log files in the output
directory.  The data is logged as normal text files (as written out on
the console) as well as a csv file and files suitable for use with
gnuplot.

For testing and debugging, the `nfp-pciebench.py` utility allows the
user to run individual tests.  Check out the help message.


### Notes on running on multi-socket systems

On modern x86 multi-socket systems each socket (node) has its own PCIe
root complex and memory controller.  Thus a PCIe access from a device
may go to local or remote memory on the host.

The `nfp_pciebench.ko` kernel module may allocate memory from a given
node, controlled via the `node` parameter:

```sh
insmod ./kernel/nfp_pciebench.ko node=1
```

You also need to control where the control program runs, using the
`taskset` utility.

On multi-socket systems, it is useful to run `lstopo` (or
`lstopo-no-graphics` depending on your distribution) to discover
the topology of CPU cores and PCIe devices.  The `-c` or `--taskset`
option are useful to determine the mask for the taskset commandline.
