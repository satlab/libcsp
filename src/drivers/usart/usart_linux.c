/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 GomSpace ApS (http://www.gomspace.com)
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

#include <csp/drivers/usart.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/serial.h>
#include <asm/termbits.h>

#include <csp/csp.h>
#include <csp/arch/csp_thread.h>

typedef struct {
	csp_usart_callback_t rx_callback;
	void * user_data;
	csp_usart_fd_t fd;
	csp_thread_handle_t rx_thread;
} usart_context_t;

static void * usart_rx_thread(void * arg) {

	usart_context_t * ctx = arg;
	const unsigned int CBUF_SIZE = 400;
	uint8_t cbuf[CBUF_SIZE];

	while (1) {
		int length = read(ctx->fd, cbuf, CBUF_SIZE);
		if (length <= 0) {
			csp_log_error("%s: read() failed, returned: %d", __FUNCTION__, length);
			break;
		}
		if (ctx->rx_callback)
			ctx->rx_callback(ctx->user_data, cbuf, length, NULL);
	}
	return NULL;
}

int csp_usart_write(csp_usart_fd_t fd, const void * data, size_t data_length) {

	if (fd >= 0) {
		int res = write(fd, data, data_length);
		if (res >= 0) {
			return res;
		}
	}
	return CSP_ERR_TX; // best matching CSP error code.

}

int csp_usart_open(const csp_usart_conf_t *conf, csp_usart_callback_t rx_callback, void * user_data, csp_usart_fd_t * return_fd) {

	struct termios2 options;
#ifdef ASYNC_LOW_LATENCY
	struct serial_struct ss;
#endif

	int fd = open(conf->device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		csp_log_error("%s: failed to open device: [%s], errno: %s", __FUNCTION__, conf->device, strerror(errno));
		return CSP_ERR_INVAL;
	}

	/* Get current configuration */
	if (ioctl(fd, TCGETS2, &options) != 0) {
		csp_log_error("%s: Failed to get attributes on device: [%s], errno: %s", __FUNCTION__, conf->device, strerror(errno));
		return CSP_ERR_DRIVER;
	}

	/* Configure for 8N1 */
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag &= ~CBAUD;
	options.c_cflag |= CS8;
	options.c_cflag |= BOTHER;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	options.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
	options.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
	options.c_cc[VTIME] = 0;
	options.c_cc[VMIN] = 1;
	options.c_ispeed = conf->baudrate;
	options.c_ospeed = conf->baudrate;

	/*
	 * Update configuration. Note that some drivers accept the requested
	 * baudrate even if is outside achievable range
	 */
	if (ioctl(fd, TCSETS2, &options) != 0) {
		csp_log_error("%s: Failed to set attributes on device: [%s], errno: %s", __FUNCTION__, conf->device, strerror(errno));
		return CSP_ERR_DRIVER;
	}

	/* Clear any file status flags */
	fcntl(fd, F_SETFL, 0);

#ifdef ASYNC_LOW_LATENCY
	/* Try to enable low latency */
	if (ioctl(fd, TIOCGSERIAL, &ss) == 0) {
		ss.flags |= ASYNC_LOW_LATENCY;
		ioctl(fd, TIOCSSERIAL, &ss);
	}
#endif

	usart_context_t * ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		csp_log_error("%s: Error allocating context, device: [%s], errno: %s", __FUNCTION__, conf->device, strerror(errno));
		close(fd);
		return CSP_ERR_NOMEM;
	}
	ctx->rx_callback = rx_callback;
	ctx->user_data = user_data;
	ctx->fd = fd;

        if (rx_callback) {
		if (csp_thread_create(usart_rx_thread, "usart_rx", 0, ctx, 0, &ctx->rx_thread) != CSP_ERR_NONE) {
			csp_log_error("%s: csp_thread_create() failed to create Rx thread for device: [%s], errno: %s", __FUNCTION__, conf->device, strerror(errno));
			free(ctx);
			close(fd);
			return CSP_ERR_NOMEM;
		}
	}

        if (return_fd) {
            *return_fd = fd;
	}

	return CSP_ERR_NONE;
}
