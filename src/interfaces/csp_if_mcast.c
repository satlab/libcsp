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
 *
 * This interface implements transmission of CSP frames over UDP multicast. The
 * default group is set to 230.74.76.80 and port 17002. The transmitted CSP
 * packets are encapsulated in UDP datagrams with the CSP header in the first 4
 * bytes. The header is sent in big endian byte order.
 *
 * Currently, the interface uses Linux system calls directly, but support for
 * other network stacks can easily be added later without breaking the current
 * API.
 *
 * The multicast socket is setup for loopback mode so multiple applications can
 * communicate on one host.
 */

#include <csp/interfaces/csp_if_mcast.h>

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/csp_interface.h>
#include <csp/arch/csp_malloc.h>
#include <csp/arch/csp_thread.h>

/* Multicast group and port */
#define CSP_MCAST_GROUP	"230.74.76.80"
#define CSP_MCAST_PORT	17002

/** Maximum Transmission Unit */
#define CSP_MCAST_MTU 	256

/* State */
struct csp_mcast_ifdata {
	char ifname[CSP_IFLIST_NAME_MAX + 1];
	csp_iface_t iface;

	int mcast_socket;
	struct sockaddr_in si_send;
	struct sockaddr_in si_recv;
	struct ip_mreq mreq;
	pthread_t rx_thread;
};

static void *csp_mcast_rx_task(void *param)
{
	struct csp_mcast_ifdata *data = param;

	ssize_t r;
	socklen_t slen = sizeof(data->si_recv);
	char buf[sizeof(uint32_t) + CSP_MCAST_MTU];

	csp_packet_t *packet;
	uint32_t id;

	while(1) {
		r = recvfrom(data->mcast_socket, buf, sizeof(buf), 0, (struct sockaddr *) &data->si_recv, &slen);

		if (r < (ssize_t)sizeof(id)) {
			data->iface.rx_error++;
			continue;
		}

		packet = csp_buffer_get(r - sizeof(id));
		if (!packet)
			continue;

		memcpy(&id, buf, sizeof(id));
		packet->id.ext = csp_ntoh32(id);
		packet->length = r - sizeof(id);
		memcpy(packet->data, &buf[sizeof(id)], packet->length);

		csp_qfifo_write(packet, &data->iface, NULL);
	}

	return NULL;
}

static int csp_mcast_tx(const csp_route_t *route, csp_packet_t *packet)
{
	struct csp_mcast_ifdata *data = route->iface->driver_data;

	ssize_t t;
	int ret = CSP_ERR_NONE;
	socklen_t slen = sizeof(data->si_recv);
	char buf[sizeof(uint32_t) + CSP_MCAST_MTU];

	uint32_t id = csp_hton32(packet->id.ext);

	memcpy(buf, &id, sizeof(id));
	memcpy(buf + sizeof(id), packet->data, packet->length);

	t = sendto(data->mcast_socket, buf, sizeof(id) + packet->length, 0, (struct sockaddr *) &data->si_send, slen);
	if (t != (ssize_t)sizeof(id) + packet->length) {
		data->iface.tx_error++;
		ret = CSP_ERR_INVAL;
	}

	csp_buffer_free(packet);

	return ret;
}

int csp_mcast_init(const char *device, const char *ifname, csp_iface_t **ifc)
{
	int reuse = 1;
	unsigned char loop = 1;

	/* Allocate interface data */
	struct csp_mcast_ifdata *data = csp_calloc(1, sizeof(*data));
	if (!data)
		return CSP_ERR_NOMEM;

	if (!ifname)
		ifname = CSP_IF_MCAST_DEFAULT_NAME;

	strncpy(data->ifname, ifname, sizeof(data->ifname) - 1);
	data->iface.name = data->ifname;
	data->iface.driver_data = data;
	data->iface.nexthop = csp_mcast_tx;
	data->iface.mtu = CSP_MCAST_MTU;

	/* Create socket */
	data->mcast_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (data->mcast_socket < 0) {
		csp_log_error("failed to create mcast socket: %s", strerror(errno));
		free(data);
		return CSP_ERR_INVAL;
	}

	/* Build receiver address configuration */
	memset(&data->si_recv, 0, sizeof(data->si_recv));
	data->si_recv.sin_family = AF_INET;
	data->si_recv.sin_port = htons(CSP_MCAST_PORT);
	data->si_recv.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Sender is identical to receiver except for source address */
	data->si_send = data->si_recv;
	data->si_send.sin_addr.s_addr = inet_addr(CSP_MCAST_GROUP);

	/* Enable port reuse so multiple applications can run on one machine */
	if (setsockopt(data->mcast_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		csp_log_error("failed to set mcast reuse option: %s", strerror(errno));
		close(data->mcast_socket);
		free(data);
		return CSP_ERR_INVAL;
	}

	/* Enable loopback */
	if (setsockopt(data->mcast_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
		csp_log_error("failed to set mcast loop option: %s", strerror(errno));
		close(data->mcast_socket);
		free(data);
		return CSP_ERR_INVAL;
	}

	/* Bind receive addr to socket */
	if (bind(data->mcast_socket, (struct sockaddr*)&data->si_recv, sizeof(data->si_recv)) < 0) {
		csp_log_error("failed to bind to mcast socket: %s", strerror(errno));
		close(data->mcast_socket);
		free(data);
		return CSP_ERR_INVAL;
	}

	/* Join multicast group */
	data->mreq.imr_multiaddr.s_addr = inet_addr(CSP_MCAST_GROUP);
	data->mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(data->mcast_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &data->mreq, sizeof(data->mreq)) < 0) {
		csp_log_error("failed to join mcast group: %s", strerror(errno));
		close(data->mcast_socket);
		free(data);
		return CSP_ERR_INVAL;
	}

	/* Start receive thread */
	if (pthread_create(&data->rx_thread, NULL, csp_mcast_rx_task, data) < 0) {
		csp_log_error("failed to create mcast rx thread: %s", strerror(errno));
		close(data->mcast_socket);
		free(data);
		return CSP_ERR_INVAL;
	}

	/* Finally, add interface to stack */
	csp_iflist_add(&data->iface);

	if (ifc)
		*ifc = &data->iface;

	return CSP_ERR_NONE;
}
