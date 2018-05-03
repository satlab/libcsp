# Copyright (c) 2016-2017 Satlab Aps <satlab@satlab.com>
# All rights reserved. Do not distribute without permission.

from __future__ import print_function

import sys
import os
import struct
import ctypes

def init(testlib_file):
    global testlib
    testlib = testlib_file

class CSPNode():
    def __init__(self):
        # Get function references from lib
        global testlib

        # void csp_reboot(uint8_t node)
        self._csp_reboot = testlib.csp_reboot
        self._csp_reboot.argtypes = [ctypes.c_uint8]
        self._csp_reboot.restype = None

        self._csp_ping = testlib.csp_ping
        self._csp_ping.argtypes = [ctypes.c_uint8, ctypes.c_uint32, ctypes.c_uint, ctypes.c_uint8]
        self._csp_ping.restype = ctypes.c_int

    def ping(self, timeout=1000, size=1, options=0):
        return self._csp_ping(self.addr, timeout, size, options)

    def reboot(self):
        self._csp_reboot(self.addr)
