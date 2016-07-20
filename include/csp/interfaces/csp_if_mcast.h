/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2016 GomSpace ApS (http://www.gomspace.com)
Copyright (C) 2016 AAUSAT3 Project (http://aausat3.space.aau.dk)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Multicast UDP interface for CSP
 * Copyright (C) 2016 Satlab ApS (http://www.satlab.com) 
 */

#ifndef _CSP_IF_MCAST_H_
#define _CSP_IF_MCAST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <csp/csp.h>
#include <csp/csp_interface.h>

/**
   Default MCAST interface name.
*/
#define CSP_IF_MCAST_DEFAULT_NAME	"MCAST"

/**
   Setup multicast interface.
   @param[in] device IP Interface to bind to for multicast traffic, NULL for any
   @param[in] ifname Name of CSP interface, use NULL for default name #CSP_IF_MCAST_DEFAULT_NAME.
   @param[out] ifc created CSP interface.
   @return #CSP_ERR_NONE on succcess - else assert.
*/
int csp_mcast_init(const char *device, const char *ifname, csp_iface_t **ifc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _CSP_IF_MCAST_H_ */
