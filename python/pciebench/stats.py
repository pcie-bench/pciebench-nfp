## Copyright (C) 2015-2018 Rolf Neugebauer.  All rights reserved.
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

"""A collection of stats functions"""

import math
import sys

class ListStats(object):
    """A class implementing some statistics on a list of values"""

    def __init__(self, inlist):
        """Initialise a stats object with a list of items"""
        self.list = inlist
        self.sorted_list = sorted(self.list)

    def avg(self):
        """Return the average of the list."""
        if not self.list:
            return 0.0
        return float(sum(self.list))/len(self.list)

    def median(self):
        """Return the median of the values."""
        if not self.list:
            return 0
        length = len(self.sorted_list)
        if not length % 2:
            return (self.sorted_list[int(length / 2)] +
                    self.sorted_list[int(length / 2 - 1)]) / 2.0
        return self.sorted_list[int(length / 2)]

    def min(self):
        """Return the minimum value in the list"""
        return min(self.list)

    def max(self):
        """Return the maximum value in the list"""
        return max(self.list)

    def percentile(self, percentile):
        """Return the nth the percentile from a list of values."""
        # from http://code.activestate.com/recipes/511478/
        if not self.list:
            return 0
        idx = (len(self.sorted_list) - 1) * (percentile / 100.0)
        floor = math.floor(idx)
        ceil = math.ceil(idx)
        if floor == ceil:
            return self.sorted_list[int(idx)]
        val0 = self.sorted_list[int(floor)] * (ceil - idx)
        val1 = self.sorted_list[int(ceil)] * (idx - floor)
        return val0 + val1

    def histo(self):
        """Return a histogram from a list.
        Returns a dictionary with values as key and #occurrences as values
        """
        res = {}
        for val in self.list:
            if not val in res:
                res[val] = 1
            else:
                res[val] += 1
        return res

def histo2cdf(histo):
    """Convert a histogram dictionary into a CDF.
    Returns a dictionary with values as keys and the CDF as values.
    The values is the fraction in the range of 0-1.0"""
    # work out total number of items
    total = 0
    for val in histo:
        total += histo[val]

    cdf_frac = 0.0
    res = {}

    sorted_keys = sorted(histo.keys())
    for val in sorted_keys:
        cdf_frac += float(histo[val]) / float(total)
        res[val] = cdf_frac
    return res

def pp_list(inlist, perl=10):
    """Pretty print a large list. @perl says how many items per line"""
    cnt = 0
    i = 0
    while cnt < len(inlist):
        print('%4d: %s' % (i, ' '.join('%8x' % s for s in
                                       inlist[i:i+perl])))
        i = (i + perl)
        cnt = cnt + perl
    sys.stdout.flush()

def pp_hist(histo, outf=sys.stdout):
    """Pretty Print a histogram"""
    if not outf:
        return
    vals = sorted(histo.keys())
    for i in vals:
        outf.write("%5d %d\n" % (i, vals[i]))

def pp_cdf(cdf, outf=sys.stdout):
    """Pretty Print a CDF dictionary"""
    if not outf:
        return
    vals = sorted(cdf.keys())
    for i in vals:
        outf.write("%5d %8.6f\n" % (i, cdf[i]))
