/*
 * Copyright (C) 2021 Satlab A/S (https://www.satlab.com)
 *
 * This file is part of CSP. See COPYING for details.
 */

#include <csp/interfaces/csp_if_sludp.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <pthread.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/csp_interface.h>
#include <csp/arch/csp_malloc.h>
#include <csp/arch/csp_thread.h>

/* MTU of the SLUDP interface */
#define CSP_IF_SLUDP_MTU	1024

/* Default UDP port */
#define CSP_IF_SLUDP_PORT	53001

struct csp_sludp_ifdata {
	char ifname[CSP_IFLIST_NAME_MAX + 1];
	csp_iface_t iface;

	int dest_socket;
	struct sockaddr_in dest_addr;
	pthread_t rx_thread;
};

static void *csp_sludp_rx(void *arg)
{
	struct csp_sludp_ifdata *data = arg;

	int nbytes;
	uint32_t id;
	uint16_t length;
	csp_packet_t *packet;
	uint8_t frame[sizeof(id) + CSP_IF_SLUDP_MTU];

	while (1) {
		/* Wait for frames */
		nbytes = recvfrom(data->dest_socket, frame, sizeof(frame), 0, NULL, NULL);
		if (nbytes < 0 || (unsigned int)nbytes < sizeof(id))
			continue;

		/* Extract CSP length */
		length = nbytes - sizeof(id);

		/* Allocate buffer for frame */
		packet = csp_buffer_get(length);
		if (!packet)
			continue;
		packet->length = length;

		/* Set packet ID */
		memcpy(&id, &frame[0], sizeof(id));
		packet->id.ext = csp_ntoh32(id);

		/* Copy received data into CSP buffer */
		memcpy(packet->data, &frame[sizeof(id)], packet->length);

		/* Pass frame to CSP stack */
		csp_qfifo_write(packet, &data->iface, NULL);
	}

	return NULL;
}

static int csp_sludp_tx(const csp_route_t *route, csp_packet_t *packet)
{
	struct csp_sludp_ifdata *data = route->iface->driver_data;

	int nbytes;
	uint32_t id;
	size_t length = 0;
	struct sockaddr_in dest = data->dest_addr;
	uint8_t destb, frame[sizeof(id) + CSP_IF_SLUDP_MTU];

	id = csp_hton32(packet->id.ext);

	memcpy(&frame[length], &id, sizeof(id));
	length += sizeof(id);
	memcpy(&frame[length], packet->data, packet->length);
	length += packet->length;

	/* Build destination address */
	destb = route->via;
	if (destb == CSP_NODE_MAC)
		destb = packet->id.dst;

	/* Last byte in destination IP is CSP address (or MAC if via node) */
	dest.sin_addr.s_addr = dest.sin_addr.s_addr | destb << 24;

	/* Send message */
	nbytes = sendto(data->dest_socket, frame, length, 0,
			(struct sockaddr *) &dest, sizeof(dest));
	if (nbytes != (int)length)
		return CSP_ERR_TIMEDOUT;

	csp_buffer_free(packet);

	return CSP_ERR_NONE;
}

int csp_sludp_init(const char *device, const char *ifname, csp_iface_t **ifc)
{
	int ret;
	struct sockaddr_in sa;
	struct ifreq ifr;

	/* Allocate interface data */
	struct csp_sludp_ifdata *data = csp_calloc(1, sizeof(*data));
	if (!data)
		return CSP_ERR_NOMEM;

	if (!ifname)
		ifname = CSP_IF_SLUDP_DEFAULT_NAME;

	strncpy(data->ifname, ifname, sizeof(data->ifname) - 1);
	data->iface.name = data->ifname;
	data->iface.driver_data = data;
	data->iface.nexthop = csp_sludp_tx;
	data->iface.mtu = CSP_IF_SLUDP_MTU;

	/* Create socket */
	data->dest_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (data->dest_socket < 0) {
		csp_log_error("failed to allocate UDP socket\n");
		return CSP_ERR_DRIVER;
	}

	/* Get interface IP address */
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, device, IFNAMSIZ-1);
	ret = ioctl(data->dest_socket, SIOCGIFADDR, &ifr);
	if (ret < 0) {
		csp_log_error("failed to get interface address\n");
		return CSP_ERR_DRIVER;
	}

	/* Bind to incoming port on interface */
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	sa.sin_port = htons(CSP_IF_SLUDP_PORT);

	ret = bind(data->dest_socket, (const struct sockaddr *)&sa, sizeof(struct sockaddr));
	if (ret < 0) {
		free(data);
		csp_log_error("failed to bind receive socket\n");
		return CSP_ERR_DRIVER;
	}

	/* Create destination address */
	memset(&data->dest_addr, 0, sizeof(data->dest_addr));
	data->dest_addr.sin_family = AF_INET;
	data->dest_addr.sin_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	data->dest_addr.sin_port = htons(CSP_IF_SLUDP_PORT);

	/* Last byte in destination is set in TX function */
	data->dest_addr.sin_addr.s_addr = data->dest_addr.sin_addr.s_addr & 0x00ffffff;

	/* Start receiver thread */
	ret = pthread_create(&data->rx_thread, NULL, csp_sludp_rx, data);
	if (ret < 0) {
		free(data);
		csp_log_error("failed to start receive thread\n");
		return CSP_ERR_DRIVER;
	}

	/* Register interface */
	csp_iflist_add(&data->iface);

	if (ifc)
		*ifc = &data->iface;

	return CSP_ERR_NONE;
}
