/*
 * Copyright (C) 2021 Satlab A/S (https://www.satlab.com)
 *
 * This file is part of CSP. See COPYING for details.
 */

#ifndef _CSP_IF_SLGND_H_
#define _CSP_IF_SLGND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <csp/csp_interface.h>

/**
   Default name of SLGND interface.
*/
#define CSP_IF_SLGND_DEFAULT_NAME	"SLGND"

/**
   Setup SLGND interface.
   @param[in] radio_host Hostname or IP address of radio host. Append :port to change from the default port.
   @param[in] ifname Name of CSP interface, use NULL for default name #CSP_IF_SLGND_DEFAULT_NAME.
   @param[out] ifc Created CSP interface.
   @return #CSP_ERR_NONE on succcess - else assert.
*/
int csp_slgnd_init(const char *radio_host, const char *ifname, csp_iface_t **ifc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _CSP_IF_SLGND_H_ */
