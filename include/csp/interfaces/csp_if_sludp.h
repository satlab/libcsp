/*
 * Copyright (C) 2021 Satlab A/S (https://www.satlab.com)
 *
 * This file is part of CSP. See COPYING for details.
 */

#ifndef _CSP_IF_SLUDP_H_
#define _CSP_IF_SLUDP_H_

#include <csp/csp.h>
#include <csp/csp_interface.h>

#define CSP_IF_SLUDP_DEFAULT_NAME	"SLUDP"

int csp_sludp_init(const char *device, const char *ifname, csp_iface_t **ifc);

#endif /* _CSP_IF_SLUDP_H_ */
