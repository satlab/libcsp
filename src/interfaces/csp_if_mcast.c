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
#include <csp/csp_interface.h>
#include <csp/csp_endian.h>


/* Multicast group and port */
#define CSP_MCAST_GROUP	"230.74.76.80"
#define CSP_MCAST_PORT	17002

/** Maximum Transmission Unit */
#define CSP_MCAST_MTU 	256

/* State */
static int csp_mcast_socket;
static struct sockaddr_in si_send;
static struct sockaddr_in si_recv;
static struct ip_mreq mreq;

static void *csp_mcast_rx_task(void *param)
{
	int r;
	socklen_t slen = sizeof(si_recv);
	char buf[sizeof(uint32_t) + CSP_MCAST_MTU];

	csp_packet_t *packet;
	uint32_t id;

	while(1) {
		r = recvfrom(csp_mcast_socket, buf, sizeof(buf), 0, (struct sockaddr *) &si_recv, &slen);

		if (r < sizeof(id)) {
			csp_if_mcast.rx_error++;
			continue;
		}

		packet = csp_buffer_get(r - sizeof(id));
		if (!packet)
			continue;

		memcpy(&id, buf, sizeof(id));
		packet->id.ext = csp_ntoh32(id);
		packet->length = r - sizeof(id);
		memcpy(packet->data, &buf[sizeof(id)], packet->length);

		csp_new_packet(packet, &csp_if_mcast, NULL);
	}

	return NULL;
}

static int csp_mcast_tx(csp_packet_t *packet, uint32_t timeout)
{
	int t, ret = CSP_ERR_NONE;
	socklen_t slen = sizeof(si_recv);
	char buf[sizeof(uint32_t) + CSP_MCAST_MTU];

	uint32_t id = csp_hton32(packet->id.ext);

	memcpy(buf, &id, sizeof(id));
	memcpy(buf + sizeof(id), packet->data, packet->length);

	t = sendto(csp_mcast_socket, buf, sizeof(id) + packet->length, 0, (struct sockaddr *) &si_send, slen);
	if (t != sizeof(id) + packet->length) {
		csp_if_mcast.tx_error++;
		ret = CSP_ERR_INVAL;
	}

	csp_buffer_free(packet);

	return ret;
}

int csp_mcast_init(const char *ifc)
{
	int reuse = 1;
	unsigned char loop = 1;
	pthread_t rx_thread;

	/* Create socket */
	csp_mcast_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (csp_mcast_socket < 0) {
		csp_log_error("failed to create mcast socket: %s", strerror(errno));
		return CSP_ERR_INVAL;
	}

	/* Build receiver address configuration */
	memset(&si_recv, 0, sizeof(si_recv));
	si_recv.sin_family = AF_INET;
	si_recv.sin_port = htons(CSP_MCAST_PORT);
	si_recv.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Sender is identical to receiver except for source address */
	si_send = si_recv;
	si_send.sin_addr.s_addr = inet_addr(CSP_MCAST_GROUP);

	/* Enable port reuse so multiple applications can run on one machine */
	if (setsockopt(csp_mcast_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		csp_log_error("failed to set mcast reuse option: %s", strerror(errno));
		close(csp_mcast_socket);
		return CSP_ERR_INVAL;
	}

	/* Enable loopback */
	if (setsockopt(csp_mcast_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
		csp_log_error("failed to set mcast loop option: %s", strerror(errno));
		close(csp_mcast_socket);
		return CSP_ERR_INVAL;
	}

	/* Bind receive addr to socket */
	if (bind(csp_mcast_socket, (struct sockaddr*)&si_recv, sizeof(si_recv)) < 0) {
		csp_log_error("failed to bind to mcast socket: %s", strerror(errno));
		close(csp_mcast_socket);
		return CSP_ERR_INVAL;
	}

	/* Join multicast group */
	mreq.imr_multiaddr.s_addr = inet_addr(CSP_MCAST_GROUP);         
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);         
	if (setsockopt(csp_mcast_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		csp_log_error("failed to join mcast group: %s", strerror(errno));
		close(csp_mcast_socket);
		return CSP_ERR_INVAL;
	}

	/* Start receive thread */
	if (pthread_create(&rx_thread, NULL, csp_mcast_rx_task, NULL) < 0) {
		csp_log_error("failed to create mcast rx thread: %s", strerror(errno));
		close(csp_mcast_socket);
		return CSP_ERR_INVAL;
	}

	/* Finally, add interface to stack */
	csp_route_add_if(&csp_if_mcast);

	return CSP_ERR_NONE;
}

csp_iface_t csp_if_mcast = {
	.name = "MCAST",
	.nexthop = csp_mcast_tx,
	.mtu = CSP_MCAST_MTU,
};
