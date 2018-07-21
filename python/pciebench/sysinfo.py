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

"""A modules which collects useful information about a system,
primarily by executing a bunch of commands and store the output in
separate files."""

import os
import subprocess
from .debug import err, warn

def _exec_redir(cmd, outfn):
    """Execute command and redirect stdout put to a file"""
    outf = open(outfn, "w")
    try:
        subprocess.call(cmd, shell=True, stdout=outf, stderr=outf)
    except subprocess.CalledProcessError:
        pass
    outf.close()

def collect(path, nfp_num=-1):
    """Collect system information and store the output in the
    directory pointed to by @path. If nfp_num is set, collect NFP
    information as well"""

    if os.path.exists(path):
        if not os.path.isdir(path):
            err("%s is not a directory" % path)
    else:
        os.makedirs(path)

    if os.geteuid() != 0:
        warn("You are not root, some commands may not work.")

    _exec_redir("date", path + "/sys-date.txt")
    _exec_redir("hostname", path + "/sys-hostname.txt")

    _exec_redir("cat /proc/cpuinfo", path + "/sys-cpuinfo.txt")
    _exec_redir("cat /proc/cmdline", path + "/sys-kernel-cmdline.txt")
    _exec_redir("free", path + "/sys-memory.txt")
    _exec_redir("cat /proc/meminfo", path + "/sys-meminfo.txt")
    _exec_redir("numactl --hardware", path + "/sys-numactl.txt")
    _exec_redir("lstopo-no-graphics -c", path + "/sys-lstopo.txt")

    _exec_redir("uname -a", path + "/sys-uname.txt")
    _exec_redir("lsb_release -a", path + "/sys-lsb_release.txt")
    _exec_redir("dmidecode", path + "/sys-dmidecode.txt")

    _exec_redir("lspci -vvv", path + "/sys-lspci.txt")

    _exec_redir("dmesg", path + "/sys-dmesg.txt")

    # On modern Intel/AMD processors PCI devices are local to a CPU
    # Extract this information from sysfs
    pcidir = "/sys/bus/pci/devices"
    if os.path.exists(pcidir):
        outf = open(path + "/sys-pci-cpulist.txt", 'w')
        for device in os.listdir(pcidir):
            devpath = pcidir + '/' + device
            if os.path.exists(devpath + '/local_cpulist'):
                with open(devpath + '/local_cpulist', 'r') as dev_file:
                    cpu_list = dev_file.read()
            else:
                cpu_list = "NA"
            if os.path.exists(devpath + '/local_cpus'):
                with open(devpath + '/local_cpus', 'r') as dev_file:
                    cpus = dev_file.read()
            else:
                cpus = "NA"
            outf.write("%s %s %s\n" % (device, cpu_list.strip(), cpus.strip()))
        outf.close()

    if not nfp_num == -1:
        _exec_redir("nfp-hwinfo -n %d" % nfp_num, path + "/sys-nfp-hwinfo.txt")
        _exec_redir("cat /proc/pciebench_dma_addrs-%d" % nfp_num,
                    path + "/sys-dma-addrs.txt")
        _exec_redir("cat /proc/pciebench_buf_sz-%d" % nfp_num,
                    path + "/sys-buf-sz.txt")


def end(path):
    """Collect system information at the end of a benchmark"""

    if os.path.exists(path):
        if not os.path.isdir(path):
            err("%s is not a directory" % path)
    else:
        os.makedirs(path)

    if os.geteuid() != 0:
        warn("You are not root, some commands may not work.")

    _exec_redir("date", path + "/sys-date-end.txt")
    _exec_redir("dmesg", path + "/sys-dmesg-end.txt")


# Test
if __name__ == '__main__':
    collect("./sysinfo")
