/*
 * Copyright (C) 2021 Satlab A/S (https://www.satlab.com)
 *
 * This file is part of CSP. See COPYING for details.
 */

#ifndef _CSP_IF_SLUDP_H_
#define _CSP_IF_SLUDP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <csp/csp_interface.h>

/**
   Default name of SLUDP interface.
*/
#define CSP_IF_SLUDP_DEFAULT_NAME	"SLUDP"

/**
   Setup SLUDP interface.
   @param[in] device Ethernet device to bind and read address prefix from.
   @param[in] ifname Name of CSP interface, use NULL for default name #CSP_IF_SLUDP_DEFAULT_NAME.
   @param[out] ifc Created CSP interface.
   @return #CSP_ERR_NONE on succcess - else assert.
*/
int csp_sludp_init(const char *device, const char *ifname, csp_iface_t **ifc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _CSP_IF_SLUDP_H_ */
