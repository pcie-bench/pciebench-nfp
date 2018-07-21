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

"""A small and simple library for debug output"""

import sys

# pylint: disable=superfluous-parens

# Verbosity level
VLVL = 0

def err(msg):
    """Print an error message and exit"""
    raise Exception(msg)

def warn(msg):
    """Print an warning message and exit"""
    print(msg)
    sys.stdout.flush()

def log(msg):
    """Print an log message and exit"""
    if VLVL and VLVL > 0:
        print(msg)
        sys.stdout.flush()

def dbg(msg):
    """Print an debug message and exit"""
    if VLVL and VLVL > 1:
        print(msg)
        sys.stdout.flush()

def trc(msg):
    """Print an trace message and exit"""
    if VLVL and VLVL > 2:
        print(msg)
        sys.stdout.flush()
