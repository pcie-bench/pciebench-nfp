#! /usr/bin/env python
#
## Copyright (C) 2015 Rolf Neugebauer.  All rights reserved.
## Copyright (C) 2015 Netronome Systems, Inc.  All rights reserved.
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##   http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.

"""Run a set of PCIe micro-benchmarks on a NFP"""

import sys
from optparse import OptionParser

from pciebench.nfpbench import NFPBench
from pciebench.tablewriter import TableWriter
from pciebench.stats import histo2cdf
import pciebench.debug
import pciebench.sysinfo


def run_lat_cmd(nfp, outdir):
    """Run basic Latency tests for different sizes using the PCIe commands"""
    twr = TableWriter(nfp.lat_fmt)
    twr.open(outdir + "lat_cmd_sizes", TableWriter.ALL)

    twr.msg("\nPCIe CMD Latency with different transfer sizes")

    win_sz = 4096
    trans_szs = [4, 8, 16, 24, 32, 48, 64]

    flags = nfp.FLAGS_HOSTWARM

    for test_no in [nfp.LAT_CMD_RD, nfp.LAT_CMD_WRRD]:
        twr.sec()
        for trans_sz in trans_szs:
            _ = nfp.lat_test(twr, test_no, flags, win_sz, trans_sz, 0, 0)

    twr.close(TableWriter.ALL)


def run_lat_cmd_sweep(nfp, outdir):
    """Run Latency tests to determine any cache or IO-MMU effects"""
    twr = TableWriter(nfp.lat_fmt)

    win_szs = [x * 1024 for x in [1, 4, 16, 256, 512]] + \
              [x * 1024 * 1024 for x in [1, 1.5, 2, 3, 4, 8, 16, 32, 64]]
    trans_szs = [8, 64]
    access = nfp.FLAGS_RANDOM

    for test_no in [nfp.LAT_CMD_RD, nfp.LAT_CMD_WRRD]:
        for trans_sz in trans_szs:

            out_name = "lat_cmd_sweep_"
            out_name += "rd_" if test_no == nfp.LAT_CMD_RD else "wrrd_"
            out_name += "seq_" if access == 0 else "rnd_"
            out_name += "%02d" % trans_sz

            twr.open(outdir + out_name, TableWriter.ALL)
            twr.msg("\n\nPCIe CMD %s latency over different windows "
                    "sizes with %s access" %
                    ("Read" if test_no == nfp.LAT_CMD_RD else "Write/Read",
                     "sequential" if access == 0 else "random"))

            for flags in [0, nfp.FLAGS_THRASH,
                          nfp.FLAGS_WARM, nfp.FLAGS_HOSTWARM]:

                twr.sec()
                for win_sz in win_szs:
                    _ = nfp.lat_test(twr, test_no, access | flags,
                                     win_sz, trans_sz, 0, 0)

            twr.close(TableWriter.ALL)

def run_lat_cmd_off(nfp, outdir):
    """Run Latency tests to with different host offset"""
    twr = TableWriter(nfp.lat_fmt)

    win_szs = [4096, 8 * 1024 * 1024]
    trans_szs = [8, 64]

    for test_no in [nfp.LAT_CMD_RD, nfp.LAT_CMD_WRRD]:
        out_name = "lat_cmd_off_%s" % \
                   ("rd" if test_no == nfp.LAT_CMD_RD else "wrrd")
        twr.open(outdir + out_name, TableWriter.ALL)
        twr.msg("\nPCIe CMD %s latency with different host offset" %
                ("Read" if test_no == nfp.LAT_CMD_RD else "Write/Read"))

        for trans_sz in trans_szs:
            for win_sz in win_szs:
                for flags in [0, nfp.FLAGS_HOSTWARM]:
                    twr.sec()
                    for off in [0, 1, 2, 3, 4, 6, 8, 16, 32, 48]:
                        _ = nfp.lat_test(twr, test_no, flags,
                                         win_sz, trans_sz, off, 0)

        twr.close(TableWriter.ALL)


def run_lat_dma(nfp, outdir):
    """Run basic Latency tests for different sizes using the PCIe commands"""
    twr = TableWriter(nfp.lat_fmt)
    twr.open(outdir + "lat_dma_sizes", TableWriter.ALL)

    twr.msg("\nPCIe DMA Latency with different transfer sizes")
    win_sz = 8192
    flags = nfp.FLAGS_HOSTWARM
    trans_szs = [4, 8, 16, 24, 32, 48, 64,
                 128, 256, 512, 768, 1024, 1280, 1520, 2048]

    for test_no in [nfp.LAT_DMA_RD, nfp.LAT_DMA_WRRD]:
        twr.sec()
        for trans_sz in trans_szs:
            _ = nfp.lat_test(twr, test_no, flags, win_sz, trans_sz, 0, 0)

    twr.close(TableWriter.ALL)

def run_lat_dma_byte(nfp, outdir):
    """Run basic Latency tests for different sizes using the PCIe commands"""
    twr = TableWriter(nfp.lat_fmt)
    twr.open(outdir + "lat_dma_sizes_byte_inc", TableWriter.ALL)

    twr.msg("\nPCIe DMA Latency with different transfer sizes")
    win_sz = 8192
    flags = nfp.FLAGS_HOSTWARM

    for mid_sz in [256, 1024]:
        trans_szs = range(mid_sz - 16, mid_sz + 16)

        for test_no in [nfp.LAT_DMA_RD, nfp.LAT_DMA_WRRD]:
            twr.sec()
            for trans_sz in trans_szs:
                _ = nfp.lat_test(twr, test_no, flags, win_sz, trans_sz, 0, 0)

    twr.close(TableWriter.ALL)


def run_lat_dma_sweep(nfp, outdir):
    """Run Latency tests to determine any cache or IO-MMU effects"""
    twr = TableWriter(nfp.lat_fmt)

    win_szs = [x * 1024 for x in [8, 64, 256, 512]] + \
              [x * 1024 * 1024 for x in [1, 1.5, 2, 4, 8, 16, 32, 64]]
    trans_sz = 64

    access = nfp.FLAGS_RANDOM

    for test_no in [nfp.LAT_DMA_RD, nfp.LAT_DMA_WRRD]:
        out_name = "lat_dma_sweep_"
        out_name += "rd_" if test_no == nfp.LAT_DMA_RD else "wrrd_"
        out_name += "seq_" if access == 0 else "rnd_"
        out_name += "%02d" % trans_sz

        twr.open(outdir + out_name, TableWriter.ALL)
        twr.msg("\n\nPCIe DMA %s latency over different windows "
                "sizes with %s access" %
                ("Read" if test_no == nfp.LAT_DMA_RD else "Write/Read",
                 "sequential" if access == 0 else "random"))

        for flags in [0, nfp.FLAGS_THRASH,
                      nfp.FLAGS_WARM, nfp.FLAGS_HOSTWARM]:
            twr.sec()
            for win_sz in win_szs:
                _ = nfp.lat_test(twr, test_no, access | flags,
                                 win_sz, trans_sz, 0, 0)

        twr.close(TableWriter.ALL)


def run_lat_dma_off(nfp, outdir):
    """Run Latency tests to with different host offset"""
    twr = TableWriter(nfp.lat_fmt)

    flags = nfp.FLAGS_HOSTWARM
    win_sz = 8192
    trans_szs = [64, 128, 256, 407, 416, 1024, 2048]
    offsets = [0, 1, 2, 3, 4, 6, 8, 12, 16, 20, 25, 32, 41, 48, 63]

    for test_no in [nfp.LAT_DMA_RD, nfp.LAT_DMA_WRRD]:
        out_name = "lat_dma_off_%s" % \
                   ("rd" if test_no == nfp.LAT_DMA_RD else "wrrd")

        twr.open(outdir + out_name, TableWriter.ALL)
        twr.msg("\nPCIe DMA %s latency with different host offset" %
                ("Read" if test_no == nfp.LAT_DMA_RD else "Write/Read"))

        for trans_sz in trans_szs:
            twr.sec()
            for off in offsets:
                _ = nfp.lat_test(twr, test_no, flags,
                                 win_sz, trans_sz, off, 0)
            twr.sec()
            for off in offsets:
                _ = nfp.lat_test(twr, test_no, flags,
                                 win_sz, trans_sz, 0, off)

        twr.close(TableWriter.ALL)

LAT_TEST_CDF_FMT = [("cycles", 8, "%d"), ("ns", 8, "%.0f"),
                    ("cdf", 10, "%.8f")]
def run_lat_details(nfp, outdir):
    """Run a longer test and perform some analysis"""

    win_szs = [8192, 64 * 1024 * 1024]
    common_flags = nfp.FLAGS_LONG | nfp.FLAGS_RANDOM

    # Commands latencies
    trans_szs = [8]
    twr = TableWriter(nfp.lat_fmt)
    twr.open(outdir + "lat_cmd_details", TableWriter.ALL)
    cdfwr = TableWriter(LAT_TEST_CDF_FMT, stdout=False)
    cdfwr.open(outdir + "lat_cmd_details_cdf", TableWriter.ALL)
    raw = open(outdir + "lat_cmd_details_raw.dat", 'w')
    twr.msg("\nPCIe CMD latencies with more details")

    for test_no in [nfp.LAT_CMD_RD, nfp.LAT_CMD_WRRD]:
        twr.sec()
        for trans_sz in trans_szs:
            for win_sz in win_szs:
                for flags in [common_flags, common_flags | nfp.FLAGS_HOSTWARM]:
                    lat_stats = nfp.lat_test(
                        twr, test_no, flags, win_sz, trans_sz, 0, 0)

                    h_cyc = lat_stats.histo()
                    cdf_cyc = histo2cdf(h_cyc)

                    vals = sorted(cdf_cyc.keys())
                    cdfwr.sec("test=%s trans_sz=%d win_sz=%d cache=%s" %
                              (nfp.TEST_NAMES[test_no], trans_sz, win_sz,
                               "hwarm" if flags & nfp.FLAGS_HOSTWARM \
                               else "cold"))
                    cdfwr.out((vals[0], nfp.cyc2ns(vals[0]), 0))
                    for val in vals:
                        cdfwr.out((val, nfp.cyc2ns(val), cdf_cyc[val]))

                    # write raw data
                    raw.write("# %s %s Winsz=%d trans_sz=%d (values in ns)\n" %
                              (nfp.TEST_NAMES[test_no],
                               "Warm" if flags & nfp.FLAGS_HOSTWARM else "Cold",
                               win_sz, trans_sz))
                    for t in lat_stats.list:
                        raw.write("%.0f\n" % nfp.cyc2ns(t))
                    raw.write("\n\n")

    cdfwr.close(TableWriter.ALL)
    twr.close(TableWriter.ALL)
    raw.close()

    # DMA latencies
    trans_szs = [64, 2048]
    twr = TableWriter(nfp.lat_fmt)
    twr.open(outdir + "lat_dma_details", TableWriter.ALL)
    cdfwr = TableWriter(LAT_TEST_CDF_FMT, stdout=False)
    cdfwr.open(outdir + "lat_dma_details_cdf", TableWriter.ALL)
    raw = open(outdir + "lat_dma_details_raw.dat", 'w')
    twr.msg("\nPCIe DMA latencies with more details")

    for test_no in [nfp.LAT_DMA_RD, nfp.LAT_DMA_WRRD]:
        twr.sec()
        for trans_sz in trans_szs:
            for win_sz in win_szs:
                for flags in [common_flags, common_flags | nfp.FLAGS_HOSTWARM]:
                    lat_stats = nfp.lat_test(
                        twr, test_no, flags, win_sz, trans_sz, 0, 0)

                    h_cyc = lat_stats.histo()
                    cdf_cyc = histo2cdf(h_cyc)

                    vals = sorted(cdf_cyc.keys())
                    cdfwr.sec("test=%s trans_sz=%d win_sz=%d cache=%s" %
                              (nfp.TEST_NAMES[test_no], trans_sz, win_sz,
                               "hwarm" if flags & nfp.FLAGS_HOSTWARM \
                               else "cold"))
                    cdfwr.out((vals[0], nfp.cyc2ns(vals[0]), 0))
                    for val in vals:
                        cdfwr.out((val, nfp.cyc2ns(val), cdf_cyc[val]))

                    # write raw data
                    raw.write("# %s %s Winsz=%d trans_sz=%d (values in ns)\n" %
                              (nfp.TEST_NAMES[test_no],
                               "Warm" if flags & nfp.FLAGS_HOSTWARM else "Cold",
                               win_sz, trans_sz))
                    for t in lat_stats.list:
                        raw.write("%.0f\n" % nfp.cyc2ns(t))
                    raw.write("\n\n")

    cdfwr.close(TableWriter.ALL)
    twr.close(TableWriter.ALL)
    raw.close()

def run_bw_dma_sz_sweep(nfp, outdir):
    """Run Bandwidth tests across different DMA sizes"""
    twr = TableWriter(nfp.bw_fmt)

    win_sz = 8192
    # kinda assume MPS of 256
    trans_szs = [16, 32, 63, 64, 65,
                 127, 128, 129, 192,
                 255, 256, 257,
                 320, 384, 511, 512, 513,
                 576, 640, 704, 767, 768, 769,
                 832, 896, 960, 1023, 1024, 1025,
                 1279, 1280, 1281,
                 1535, 1536, 1537,
                 1791, 1792, 1793,
                 2047, 2048]

    flags = nfp.FLAGS_RANDOM | nfp.FLAGS_HOSTWARM

    out_name = "bw_dma_sz_sweep"
    twr.open(outdir + out_name, TableWriter.ALL)

    for test_no in [nfp.BW_DMA_RD, nfp.BW_DMA_WR, nfp.BW_DMA_RW]:
        twr.sec()
        for trans_sz in trans_szs:
            nfp.bw_test(twr, test_no, flags, win_sz, trans_sz, 0, 0)

    twr.close(TableWriter.ALL)


def run_bw_dma_win_sweep(nfp, outdir):
    """Run Bandwidth tests with differnt windows sizes"""
    twr = TableWriter(nfp.bw_fmt)

    win_szs = [x * 1024 for x in [4, 16, 256, 512]] + \
              [x * 1024 * 1024 for x in [1, 1.5, 2, 3, 4, 8, 16, 32, 64]]
    trans_szs = [64, 128, 256, 512]
    access = nfp.FLAGS_RANDOM

    for test_no in [nfp.BW_DMA_RD, nfp.BW_DMA_WR]:
        for trans_sz in trans_szs:

            out_name = "bw_dma_win_sweep_"
            out_name += "rd" if test_no == nfp.BW_DMA_RD else "wr"
            out_name += "_rnd_"
            out_name += "%02d" % trans_sz

            twr.open(outdir + out_name, TableWriter.ALL)
            twr.msg("\n\nPCIe DMA %s bandwidth over different windows "
                    "sizes with Random access" %
                    ("Read" if test_no == nfp.BW_DMA_RD else "Write"))
            for flags in [0, nfp.FLAGS_THRASH,
                          nfp.FLAGS_WARM, nfp.FLAGS_HOSTWARM]:

                twr.sec()
                for win_sz in win_szs:
                    nfp.bw_test(twr, test_no, flags | access,
                                win_sz, trans_sz, 0, 0)
            twr.close(TableWriter.ALL)

def run_bw_dma_off(nfp, outdir):
    """Run Bandwidth tests to with different host offset"""
    twr = TableWriter(nfp.bw_fmt)

    win_sz = 8192
    trans_szs = [64, 128, 256, 407, 416, 1024, 2048]
    offsets = [0, 1, 2, 3, 4, 6, 8, 12, 16, 20, 25, 32, 41, 48, 63]

    for flags in [0, nfp.FLAGS_HOSTWARM]:
        for test_no in [nfp.BW_DMA_RD, nfp.BW_DMA_WR]:
            out_name = "bw_dma_off_%s" % \
                       ("rd" if test_no == nfp.BW_DMA_RD else "wr")

            # With DDIO, no point in running cold/warm WRRD tests
            if flags == 0 and test_no == nfp.LAT_DMA_WRRD:
                continue

            if flags == 0:
                out_name += "_cold"

            twr.open(outdir + out_name, TableWriter.ALL)
            twr.msg("\nPCIe DMA %s Bandwidth with different host offset" %
                    ("Read" if test_no == nfp.LAT_DMA_RD else "Write"))

            for trans_sz in trans_szs:
                twr.sec()
                for off in offsets:
                    _ = nfp.bw_test(twr, test_no, flags,
                                    win_sz, trans_sz, off, 0)
                twr.sec()
                for off in offsets:
                    _ = nfp.bw_test(twr, test_no, flags,
                                    win_sz, trans_sz, 0, off)

        twr.close(TableWriter.ALL)

def run_dbg_lat(nfp, dma, write_read, win_sz, trans_sz,
                h_off, d_off, rnd, long_run, cache_flags, outdir):
    """Run latency debug test"""
    twr = TableWriter(nfp.lat_fmt)
    twr.open(outdir + "dbg_lat", TableWriter.ALL)

    cdfwr = TableWriter(LAT_TEST_CDF_FMT, stdout=False)
    cdfwr.open(outdir + "dbg_lat_details_cdf", TableWriter.ALL)

    flags = cache_flags

    if rnd:
        flags |= nfp.FLAGS_RANDOM

    if long_run:
        flags |= nfp.FLAGS_LONG

    if dma:
        if write_read:
            test_no = nfp.LAT_DMA_WRRD
        else:
            test_no = nfp.LAT_DMA_RD
    else:
        if write_read:
            test_no = nfp.LAT_CMD_WRRD
        else:
            test_no = nfp.LAT_CMD_RD

    lat_stats = nfp.lat_test(twr, test_no, flags, win_sz,
                             trans_sz, h_off, d_off)

    h_cyc = lat_stats.histo()
    cdf_cyc = histo2cdf(h_cyc)

    vals = sorted(cdf_cyc.keys())
    cdfwr.sec("test=%s trans_sz=%d win_sz=%d" %
              (nfp.TEST_NAMES[test_no], trans_sz, win_sz))
    cdfwr.out((vals[0], nfp.cyc2ns(vals[0]), 0))
    for val in vals:
        cdfwr.out((val, nfp.cyc2ns(val), cdf_cyc[val]))

    cdfwr.close(TableWriter.ALL)
    twr.close(TableWriter.ALL)


def run_dbg_bw(nfp, wr_flag, rw_flag, win_sz, trans_sz,
               h_off, d_off, rnd, cache_flags, outdir):
    """Run bandwidth debug test"""

    if wr_flag and rw_flag:
        raise Exception("Illegal combination of flags")

    if wr_flag:
        test_no = nfp.BW_DMA_WR
    elif rw_flag:
        test_no = nfp.BW_DMA_RW
    else:
        test_no = nfp.BW_DMA_RD

    twr = TableWriter(nfp.bw_fmt)
    twr.open(outdir + "dbg_bw", TableWriter.ALL)

    flags = cache_flags

    if rnd:
        flags |= nfp.FLAGS_RANDOM

    nfp.bw_test(twr, test_no, flags, win_sz, trans_sz, h_off, d_off)
    twr.close(TableWriter.ALL)


def run_dbg_mem(nfp, outdir):
    """Debug memory, trying to hit the same cachelines over and over"""

    twr = TableWriter(nfp.bw_fmt)
    twr.open(outdir + "dbg_mem", TableWriter.ALL)

    flags = nfp.FLAGS_HOSTWARM
    test_no = nfp.BW_DMA_RD
    trans_szs = [64, 128, 256, 512, 1024]
    for trans_sz in trans_szs:
            nfp.bw_test(twr, test_no, flags, trans_sz, trans_sz, 0, 0)
            nfp.bw_test(twr, test_no, flags, 8192, trans_sz, 0, 0)
    twr.close(TableWriter.ALL)


def main():
    """Main function"""

    usage = """usage: %prog [options]"""

    parser = OptionParser(usage)
    parser.add_option('-f', '--fwfile',
                      default=None, action='store', metavar='FILE',
                      help='Firmware file to use')
    parser.add_option('-n', '--nfp',
                      default=0, action='store', type='int', metavar='NUM',
                      help='select NFP device')
    parser.add_option('-o', '--outdir',
                      default="./", action='store', metavar='DIRECTORY',
                      help='Directory where to write data files')
    parser.add_option('-u', '--user-helper', dest='helper',
                      default=None, action='store', metavar='HELPER',
                      help='Path to helper binary')
    parser.add_option('-s', '--short',
                      action="store_true", dest='short', default=False,
                      help='Run a subset of the benchmarks')


    ##
    ## Debug options
    ##
    parser.add_option('--dbg-lat-cmd',
                      action="store_true", dest='dbg_lat_cmd', default=False,
                      help='Debug: Command latency debug run')
    parser.add_option('--dbg-lat-dma',
                      action="store_true", dest='dbg_lat_dma', default=False,
                      help='Debug: DMA latency debug run')
    parser.add_option('--dbg-bw-dma',
                      action="store_true", dest='dbg_bw_dma', default=False,
                      help='Debug: DMA Bandwidth debug run')
    parser.add_option('--dbg-bw',
                      action="store_true", dest='dbg_bw', default=False,
                      help='Debug: DMA Bandwidth debug sweep')

    parser.add_option('--dbg-winsz', type='int',
                      default=4096, metavar='WINSZ', dest='dbg_winsz',
                      help='Debug: Transaction size (default 4096B)')
    parser.add_option('--dbg-sz', type='int',
                      default=8, metavar='TRANSSZ', dest='dbg_transsz',
                      help='Debug: Transaction size (default 8B)')

    parser.add_option('--dbg-hoff', type='int',
                      default=0, metavar='HOFF', dest='dbg_hoff',
                      help='Debug: Host offset (default 0)')
    parser.add_option('--dbg-doff', type='int',
                      default=0, metavar='DOFF', dest='dbg_doff',
                      help='Debug: Device offset (default 0)')

    parser.add_option('--dbg-wrrd',
                      action="store_true", dest="dbg_lat_wrrd", default=False,
                      help='Debug LAT: Use write followed by read ' + \
                      '(default read)')
    parser.add_option('--dbg-wr',
                      action="store_true", dest="dbg_bw_wr", default=False,
                      help='Debug BW: Use DMA writes (default read)')
    parser.add_option('--dbg-rw',
                      action="store_true", dest="dbg_bw_rw", default=False,
                      help='Debug BW: Alternate between DMA read write')
    parser.add_option('--dbg-rnd',
                      action="store_true", dest="dbg_rnd", default=False,
                      help='Debug: Random addressing (default sequential)')
    parser.add_option('--dbg-long',
                      action="store_true", dest="dbg_long", default=False,
                      help='Debug: Do long run')
    parser.add_option('--dbg-details',
                      action="store_true", dest="dbg_details", default=False,
                      help='Debug: Run the details test only')
    parser.add_option('--dbg-cache',
                      default=None, metavar='CACHE', dest='dbg_cache',
                      help='Debug: Cache settings hwarm|dwarm|thrash ' + \
                           '(default None)')
    parser.add_option('--dbg-mem',
                      action='store_true', dest='dbg_mem', default=False,
                      help='Debug: Hit the same cachelines over and over ' + \
                           '[window = transfersize] (default None)')

    parser.add_option("-v", '--verbose',
                      action="count", help='set the verbosity level')

    (options, _) = parser.parse_args()

    pciebench.debug.VLVL = options.verbose

    outdir = options.outdir
    if not outdir.endswith('/'):
        outdir += '/'

    # System information
    pciebench.sysinfo.collect(outdir, options.nfp)

    nfp = NFPBench(options.nfp, options.fwfile, options.helper)

    cache_vals = {'hwarm' : nfp.FLAGS_HOSTWARM,
                  'dwarm' : nfp.FLAGS_WARM,
                  'thrash' : nfp.FLAGS_THRASH}

    cache_flags = 0
    if options.dbg_cache:
        cache_flags = cache_vals[options.dbg_cache]

    if options.dbg_bw:
        run_bw_dma_sz_sweep(nfp, outdir)
        return

    if options.dbg_lat_cmd:
        run_dbg_lat(nfp, False, options.dbg_lat_wrrd,
                    options.dbg_winsz, options.dbg_transsz,
                    options.dbg_hoff, 0, options.dbg_rnd, options.dbg_long,
                    cache_flags, outdir)
        return

    if options.dbg_lat_dma:
        run_dbg_lat(nfp, True, options.dbg_lat_wrrd,
                    options.dbg_winsz, options.dbg_transsz,
                    options.dbg_hoff, options.dbg_doff,
                    options.dbg_rnd, options.dbg_long,
                    cache_flags, outdir)
        return

    if options.dbg_bw_dma:
        run_dbg_bw(nfp, options.dbg_bw_wr, options.dbg_bw_rw,
                   options.dbg_winsz, options.dbg_transsz,
                   options.dbg_hoff, options.dbg_doff,
                   options.dbg_rnd, cache_flags, outdir)
        return

    if options.dbg_details:
        run_lat_details(nfp, outdir)
        return

    if options.dbg_mem:
        run_dbg_mem(nfp, outdir)
        return

    run_lat_cmd(nfp, outdir)
    run_lat_cmd_sweep(nfp, outdir)
    if not options.short:
        run_lat_cmd_off(nfp, outdir)

    run_lat_dma(nfp, outdir)
    if not options.short:
        run_lat_dma_byte(nfp, outdir)
    run_lat_dma_sweep(nfp, outdir)
    if not options.short:
        run_lat_dma_off(nfp, outdir)

    run_lat_details(nfp, outdir)

    run_bw_dma_sz_sweep(nfp, outdir)
    run_bw_dma_win_sweep(nfp, outdir)
    if not options.short:
        run_bw_dma_off(nfp, outdir)

    pciebench.sysinfo.end(outdir)

if __name__ == '__main__':
    sys.exit(main())
