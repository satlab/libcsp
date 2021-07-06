/*
 * Copyright (C) 2021 Satlab A/S (https://www.satlab.com)
 *
 * This file is part of CSP. See COPYING for details.
 */

#ifndef _CSP_IF_SLGND_H_
#define _CSP_IF_SLGND_H_

#include <csp/csp.h>
#include <csp/csp_interface.h>

#define CSP_IF_SLGND_DEFAULT_NAME	"SLGND"

int csp_slgnd_init(const char *radio_host, const char *ifname, csp_iface_t **ifc);

#endif /* _CSP_IF_SLGND_H_ */
