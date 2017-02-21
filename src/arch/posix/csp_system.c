/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 Gomspace ApS (http://www.gomspace.com)
Copyright (C) 2012 AAUSAT3 Project (http://aausat3.space.aau.dk) 

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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <sys/reboot.h>
#include <linux/reboot.h>

#include <csp/csp.h>
#include <csp/csp_error.h>

#include <csp/arch/csp_system.h>

int csp_sys_tasklist(char * out)
{
	strcpy(out, "Tasklist not available on POSIX");
	return CSP_ERR_NONE;
}

int csp_sys_tasklist_size(void)
{
	return 100;
}

uint32_t csp_sys_memfree(void)
{
	uint32_t total = 0;
	struct sysinfo info;
	sysinfo(&info);
	total = info.freeram * info.mem_unit;
	return total;
}

int csp_sys_reboot(void)
{
#ifdef CSP_USE_INIT_SHUTDOWN
	/* Let init(1) handle the reboot */
	int ret = system("reboot");
	(void) ret; /* Silence warning */
#else
	int magic = LINUX_REBOOT_CMD_RESTART;

	/* Sync filesystem before reboot */
	sync();
	reboot(magic);
#endif

	/* If reboot(2) returns, it is an error */
	csp_log_error("Failed to reboot: %s", strerror(errno));

	return CSP_ERR_INVAL;
}

int csp_sys_shutdown(void)
{
#ifdef CSP_USE_INIT_SHUTDOWN
	/* Let init(1) handle the shutdown */
	int ret = system("halt");
	(void) ret; /* Silence warning */
#else
	int magic = LINUX_REBOOT_CMD_HALT;

	/* Sync filesystem before reboot */
	sync();
	reboot(magic);
#endif

	/* If reboot(2) returns, it is an error */
	csp_log_error("Failed to shutdown: %s", strerror(errno));

	return CSP_ERR_INVAL;
}
