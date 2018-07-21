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

"""A class for writing table like data in a number of formats"""

import sys
import csv

def sz2unit(num):
    """Convert a size into a string with units"""
    if num < 1024:
        out_num = num
        out_unit = "B"
    if num < (1024 * 1024):
        out_num = num / 1024
        out_unit = "KB"
    else:
        out_num = num / (1024 * 1024)
        out_unit = "MB"
    if int(out_num) == out_num:
        return "%d%s" % (int(out_num), out_unit)
    else:
        return "%s%s" % (out_num, out_unit)

def ns2unit(num):
    """Convert a nanosecond time a string with units"""
    if num < 1000:
        out_num = num
        out_unit = "ns"
    if num < (1000 * 1000):
        out_num = num / 1000
        out_unit = "us"
    else:
        out_num = num / (1000 * 1000)
        out_unit = "ms"
    if int(out_num) == out_num:
        return "%d%s" % (int(out_num), out_unit)
    else:
        return "%.2f%s" % (out_num, out_unit)


class TableWriter(object):
    """A class for pretty-printing table like data to stdout while
    also logging the data in files in various format"""

    # Supported file formats. Must be a unique bit
    # @TXT: Write output to a text file, pretty printed
    # @GNP: Write data in a format suitable for gnuplot
    # @CSV: Write data as a CSV file
    TXT = (1 << 0)
    GNP = (1 << 1)
    CSV = (1 << 2)

    ALL = TXT | GNP | CSV

    def __init__(self, fmt, stdout=True):
        """Initialise the TableWriter

        @fmt is a list of tuples.  Each item in the list describes a
        column.  The tuple has the form ['name', width, 'format
        string'].  The width is column width used when writing to
        stdout.  The format string is a standard python format string,
        like '%s' or '%.1f'.  The column width argument is spliced
        into the string after the '%'.

        Some special formatting strings are also supported:
        '%z':  The value is a number but for screen printing is converted
               into size units, like KB, or MB.
        '%t':  The value is a time in nanoseconds but for screen printing
               is converted into ns, us, or ms

        An empty entry ('', 0, '') creates a column separator in some
        output formats.

        @stdout controls if the data is printed to the screen
        (default).  Setting this to false is useful for just logging
        data to files.

        By setting format to None, you get a dummy writer object
        """

        # output streams
        self.std = None
        self.txt = None
        self.gnp = None
        self.csvf = None
        self.csv = None

        self.out_hdr_printed = False
        self.txt_first_sec = True
        self.gnp_first_sec = True

        self.format = None
        if fmt == None:
            return

        if stdout:
            self.std = sys.stdout

        # Convert the format.
        # Create a format string output formats which need it
        self.format = []
        self.std_hdr = self.std_fmt = ""
        for col in fmt:
            if col[0] == '' and col[1] == 0:
                self.std_hdr += "  "
                self.std_fmt += "  "
                continue

            self.format.append(col)
            self.std_hdr += "%*s " % (col[1], col[0])

            fmt = col[2].split('%')[1]
            if fmt in ['z', 'b', 't']:
                fmt = 's'
            self.std_fmt += "%%%d%s " % (col[1], fmt)

        self.std_hdr = self.std_hdr[:-1] + '\n'
        self.std_fmt = self.std_fmt[:-1] + '\n'

        self.txt_hdr = self.std_hdr
        self.gnp_hdr = "#" + self.std_hdr
        self.gnp_fmt = self.std_fmt

    def open(self, base, mask):
        """Open a data file.
        @base     Base file/path name. Extensions will be added
        @mask     Mask of file formats (GNP and or CSV)

        If a data stream for a given file is already open, close it
        first before opening a new one.

        Also write the header to the file
        """
        if self.format == None:
            return

        if mask & self.TXT:
            if self.txt:
                self.txt.close()
            self.txt = open(base + '.txt', 'w')
            self.txt.write(self.txt_hdr)
            self.txt_first_sec = True

        if mask & self.GNP:
            if self.gnp:
                self.gnp.close()
            self.gnp = open(base + '.dat', 'w')
            self.gnp.write(self.gnp_hdr)
            self.gnp_first_sec = True

        if mask & self.CSV:
            if self.csvf:
                self.csvf.close()
            self.csvf = open(base + '.csv', 'w')
            self.csv = csv.writer(self.csvf)
            self.csv.writerow([x[0] for x in self.format])

    def close(self, mask):
        """Open a data file."""
        if self.format == None:
            return
        if mask & self.TXT:
            if self.txt:
                self.txt.close()
        if mask & self.GNP:
            if self.gnp:
                self.gnp.close()
        if mask & self.CSV:
            if self.csvf:
                self.csvf.close()

    def out(self, vals):
        """Print a tuple/list of values and write them to all open files"""
        if self.format == None:
            return

        tmp = ()
        for i in range(len(vals)):
            if self.format[i][2] == '%z':
                tmp += sz2unit(vals[i]),
            elif self.format[i][2] == '%t':
                tmp += ns2unit(vals[i]),
            else:
                tmp += vals[i],

        if self.std:
            if not self.out_hdr_printed:
                self.std.write(self.std_hdr)
                self.out_hdr_printed = True
            self.std.write(self.std_fmt % tmp)
            self.std.flush()

        if self.txt:
            self.txt.write(self.std_fmt % tmp)
        if self.gnp:
            self.gnp.write(self.gnp_fmt % vals)
        if self.csv:
            self.csv.writerow(vals)

    def sec(self, head=None):
        """Some data file allow sections. This creates a new section
        with an optional section heading"""
        if self.format == None:
            return
        if self.std:
            if head:
                self.std.write("\n%s\n" % head)
            else:
                self.std.write("\n")
            self.std.write(self.std_hdr)
            self.std.flush()
            self.out_hdr_printed = True

        if self.txt:
            if self.txt_first_sec:
                if head:
                    self.txt.write("%s\n" % head)
                self.txt_first_sec = False
            else:
                if head:
                    self.txt.write("\n%s\n" % head)
                else:
                    self.txt.write("\n")

        if self.gnp:
            if self.gnp_first_sec:
                if head:
                    self.gnp.write("#%s\n" % head)
                self.gnp_first_sec = False
            else:
                if head:
                    self.gnp.write("\n\n#%s\n" % head)
                else:
                    self.gnp.write("\n\n")

    def msg(self, msg):
        """Print a message to standard out"""
        if self.std:
            self.std.write(msg)
            self.std.flush()
