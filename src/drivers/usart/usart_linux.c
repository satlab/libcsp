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
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/serial.h>
#include <asm/termbits.h>

#include <csp/csp.h>

static int csp_usart_fd;
static csp_usart_callback_t csp_usart_callback = NULL;

static void *serial_rx_thread(void *vptr_args)
{
	unsigned int length;
	uint8_t cbuf[256];

	while (1) {
		length = read(csp_usart_fd, cbuf, sizeof(cbuf));
		if (length <= 0) {
			csp_log_error("USART read error");
			break;
		}
		if (csp_usart_callback)
			csp_usart_callback(cbuf, length, NULL);
	}

	return NULL;
}

int csp_usart_init(struct csp_usart_conf *conf)
{
	struct termios2 options;
	pthread_t rx_thread;
#ifdef ASYNC_LOW_LATENCY
	struct serial_struct ss;
#endif

	csp_usart_fd = open(conf->device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (csp_usart_fd < 0)
		return CSP_ERR_DRIVER;

	/* Get current configuration */
	if (ioctl(csp_usart_fd, TCGETS2, &options) != 0) {
		csp_log_error("Error getting USART options");
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
	if (ioctl(csp_usart_fd, TCSETS2, &options) != 0) {
		csp_log_error("Error setting USART options");
		return CSP_ERR_DRIVER;
	}

	/* Clear any file status flags */
	fcntl(csp_usart_fd, F_SETFL, 0);

#ifdef ASYNC_LOW_LATENCY
	/* Try to enable low latency */
	if (ioctl(csp_usart_fd, TIOCGSERIAL, &ss) == 0) {
		ss.flags |= ASYNC_LOW_LATENCY;
		ioctl(csp_usart_fd, TIOCSSERIAL, &ss);
	}
#endif

	if (pthread_create(&rx_thread, NULL, serial_rx_thread, NULL) != 0)
		return CSP_ERR_DRIVER;

	return CSP_ERR_NONE;
}

void csp_usart_set_callback(csp_usart_callback_t callback)
{
	csp_usart_callback = callback;
}

void csp_usart_putc(char c)
{
	write(csp_usart_fd, &c, 1);
}
