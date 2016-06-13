/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2017 CSP Contributors (http://www.libcsp.org) 

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

/* CSP performance testing tool for Linux.
 *
 * The tool is basically an application wrapper around the portable csp_perf
 * functions. The command line arguments and usage is heavily inspired by iperf
 * and friends.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include <getopt.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/csp_perf.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_time.h>

static void usage(const char *argv0)
{
	printf("usage: %s [-s|-c host]\n", argv0);
}

static void help(const char *argv0)
{
	usage(argv0);
	printf("  -a, --address ADDRESS    Set address\r\n");
	printf("  -b, --bandwidth BPS      Set transmit bandwidth\r\n");
	printf("  -c, --client SERVER      Run in client mode\n");
	printf("  -p, --port PORT          Set port\n");
	printf("  -i, --interface DEV      Set CAN interface\n");
	printf("  -T, --timeout MS         Set timeout\n");
	printf("  -t, --time S             Set runtime\n");
	printf("  -h, --help               Print help and exit\r\n");
	printf("  -o, --option             Set connection options\r\n");
	printf("  -z, --size               Set payload data size\r\n");
	printf("  -s, --server             Run in server mode\n");
}

static uint8_t parse_flags(const char *flagstr)
{
	uint8_t flags = CSP_O_NONE;

	if (strchr(flagstr, 'r'))
		flags |= CSP_O_RDP;
	if (strchr(flagstr, 'c'))
		flags |= CSP_O_CRC32;
	if (strchr(flagstr, 'h'))
		flags |= CSP_O_HMAC;
	if (strchr(flagstr, 'x'))
		flags |= CSP_O_XTEA;

	return flags;
}

int main(int argc, char **argv)
{
	int ret, c;
	int option_index = 0;
	csp_conf_t csp_conf;
	const char *can_device = "can0";
	csp_iface_t *default_iface;

	struct csp_perf_config perf_conf;

	int server_mode = 0;
	int client_mode = 0;

	struct option long_options[] = {
		{"address",   required_argument, NULL,        'a'},
		{"bandwidth", required_argument, NULL,        'b'},
		{"client",    required_argument, NULL,        'c'},
		{"port",      required_argument, NULL,        'p'},
		{"interface", required_argument, NULL,        'i'},
		{"timeout",   required_argument, NULL,        'T'},
		{"time",      required_argument, NULL,        't'},
		{"help",      no_argument,       NULL,        'h'},
		{"option",    required_argument, NULL,        'o'},
		{"size",      required_argument, NULL,        'z'},
		{"server",    no_argument,       &server_mode, 1},
	};

	csp_conf_get_defaults(&csp_conf);
	csp_perf_set_defaults(&perf_conf);

	while ((c = getopt_long(argc, argv, "a:b:c:i:o:p:st:T:hz:",
				long_options, &option_index)) != -1) {
		switch (c) {
		case 'a':
			csp_conf.address = atoi(optarg);
			break;
		case 'b':
			perf_conf.bandwidth = atoi(optarg);
			break;
		case 'c':
			client_mode = 1;
			perf_conf.server = atoi(optarg);
			break;
		case 'p':
			perf_conf.port = atoi(optarg);
			break;
		case 'i':
			can_device = optarg;
			break;
		case 's':
			server_mode = 1;
			break;
		case 't':
			perf_conf.runtime = atoi(optarg);
			break;
		case 'T':
			perf_conf.timeout_ms = atoi(optarg);
			break;
		case 'o':
			perf_conf.flags = parse_flags(optarg);
			break;
		case 'h':
			help(argv[0]);
			exit(EXIT_SUCCESS);
		case 'z':
			perf_conf.data_size = atoi(optarg);
			break;
		case '?':
		default:
			exit(EXIT_FAILURE);
		}
	}

	/* Either server or client must be selected */
	if (!(server_mode ^ client_mode)) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	csp_conf.buffers = 100;
	csp_conf.buffer_data_size = 512;

	/* Init CSP */
	ret = csp_init(&csp_conf);
	if (ret != CSP_ERR_NONE)
		return EXIT_FAILURE;

	/* Setup CAN interface */
	ret = csp_can_socketcan_open_and_add_interface(can_device, CSP_IF_CAN_DEFAULT_NAME, 0, false, &default_iface);
        if (ret != CSP_ERR_NONE) {
		return EXIT_FAILURE;
        }

	csp_rtable_set(CSP_DEFAULT_ROUTE, 0, default_iface, CSP_NO_VIA_ADDRESS);
	csp_route_start_task(0, 0);

	/* Install SIGINT handler */
	//signal(SIGINT, sighandler);

	if (server_mode) {
		ret = csp_perf_server(&perf_conf);
	} else {
		ret = csp_perf_client(&perf_conf);
	}

	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
