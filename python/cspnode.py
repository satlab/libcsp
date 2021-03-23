# Copyright (c) 2016-2021 Satlab A/S <satlab@satlab.com>
# All rights reserved. Do not distribute without permission.

import os
import ctypes

# Load test library
testlib = ctypes.CDLL(os.environ['SL_TESTLIB'])

# void csp_reboot(uint8_t node)
_csp_reboot = testlib.csp_reboot
_csp_reboot.argtypes = [ctypes.c_uint8]
_csp_reboot.restype = None

# int csp_ping(uint8_t node, uint32_t timeout, unsigned int size, uint8_t conn_options)
_csp_ping = testlib.csp_ping
_csp_ping.argtypes = [ctypes.c_uint8, ctypes.c_uint32, ctypes.c_uint, ctypes.c_uint8]
_csp_ping.restype = ctypes.c_int

class CSPNode():
    def __init__(self, addr):
        self.addr = addr

    def ping(self, timeout=1000, size=1, options=0):
        return _csp_ping(self.addr, timeout, size, options)

    def reboot(self):
        _csp_reboot(self.addr)
