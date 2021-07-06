/*
 * Copyright (C) 2021 Satlab A/S (https://www.satlab.com)
 *
 * This file is part of CSP. See COPYING for details.
 */

#include <csp/interfaces/csp_if_slgnd.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pthread.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/csp_interface.h>
#include <csp/arch/csp_malloc.h>
#include <csp/arch/csp_thread.h>

/* SLGND default port */
#define CSP_IF_SLGND_PORT	"52002"

/* The MTU is actually defined by the radio configuration */
#define CSP_IF_SLGND_MTU	1024

struct csp_slgnd_ifdata {
	char ifname[CSP_IFLIST_NAME_MAX + 1];
	csp_iface_t iface;

	int radio_socket;
	struct addrinfo *radio_addr;
	pthread_t rx_thread;
};

static void *csp_slgnd_rx(void *arg)
{
	struct csp_slgnd_ifdata *data = arg;

	int nbytes;
	uint32_t id;
	uint16_t length;
	csp_packet_t *packet;
	uint8_t frame[sizeof(id) + CSP_IF_SLGND_MTU];

	while (1) {
		/* Wait for frames from radio */
		nbytes = recvfrom(data->radio_socket, frame, sizeof(frame), 0, NULL, NULL);
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

static int csp_slgnd_tx(const csp_route_t *route, csp_packet_t *packet)
{
	struct csp_slgnd_ifdata *data = route->iface->driver_data;

	int nbytes;
	uint32_t id;
	uint8_t frame[sizeof(id) + CSP_IF_SLGND_MTU];
	size_t frame_length = 0;

	id = csp_hton32(packet->id.ext);

	memcpy(&frame[frame_length], &id, sizeof(id));
	frame_length += sizeof(id);
	memcpy(&frame[frame_length], packet->data, packet->length);
	frame_length += packet->length;

	/* Send message to radio */
	nbytes = sendto(data->radio_socket, frame, frame_length, 0,
			data->radio_addr->ai_addr, data->radio_addr->ai_addrlen);
	if (nbytes != (int)frame_length)
		return CSP_ERR_TIMEDOUT;

	csp_buffer_free(packet);

	return CSP_ERR_NONE;
}

int csp_slgnd_init(const char *radio_host, const char *ifname, csp_iface_t **ifc)
{
	int ret;
	struct addrinfo hints;
	char *port, host[HOST_NAME_MAX];

	/* Allocate interface data */
	struct csp_slgnd_ifdata *data = csp_calloc(1, sizeof(*data));
	if (!data)
		return CSP_ERR_NOMEM;

	if (!ifname)
		ifname = CSP_IF_SLGND_DEFAULT_NAME;

	strncpy(data->ifname, ifname, sizeof(data->ifname) - 1);
	data->iface.name = data->ifname;
	data->iface.driver_data = data;
	data->iface.nexthop = csp_slgnd_tx;
	data->iface.mtu = CSP_IF_SLGND_MTU;

	/* Prepare host */
	strncpy(host, radio_host, sizeof(host) - 1);
	host[sizeof(host) - 1] = '\0';

	/* Use port if specified in host string */
	port = strchr(host, ':');
	if (port) {
		*port = '\0';
		port = port + 1;
	} else {
		port = (char *)CSP_IF_SLGND_PORT;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; /* GNU Radio Socket PDU only supports IPv4 */
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_ADDRCONFIG;

	/* Look up host info */
	ret = getaddrinfo(host, port, &hints, &data->radio_addr);
	if (ret < 0) {
		free(data);
		csp_log_error("failed to look up host info for %s\n", radio_host);
		return CSP_ERR_DRIVER;
	}

	/* Create radio socket */
	data->radio_socket = socket(data->radio_addr->ai_family,
				    data->radio_addr->ai_socktype,
				    data->radio_addr->ai_protocol);
	if (data->radio_socket < 0) {
		free(data);
		csp_log_error("failed to allocate radio socket\n");
		return CSP_ERR_DRIVER;
	}

	/* Start receiver thread */
	ret = pthread_create(&data->rx_thread, NULL, csp_slgnd_rx, data);
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
