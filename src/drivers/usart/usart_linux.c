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
#include <termios.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/serial.h>

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
			perror("USART read error");
			break;
		}
		if (csp_usart_callback)
			csp_usart_callback(cbuf, length, NULL);
	}

	return NULL;
}

int csp_usart_init(struct csp_usart_conf *conf)
{
	struct termios options;
	pthread_t rx_thread;
	int brate = 0;
#ifdef ASYNC_LOW_LATENCY
	struct serial_struct ss;
#endif

	csp_usart_fd = open(conf->device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (csp_usart_fd < 0)
		return CSP_ERR_DRIVER;

	switch(conf->baudrate) {
#ifdef B50
	case 50:      brate = B50; break;
#endif
#ifdef B75
	case 75:      brate = B75; break;
#endif
#ifdef B110
	case 110:     brate = B110; break;
#endif
#ifdef B134
	case 134:     brate = B134; break;
#endif
#ifdef B150
	case 150:     brate = B150; break;
#endif
#ifdef B200
	case 200:     brate = B200; break;
#endif
#ifdef B300
	case 300:     brate = B300; break;
#endif
#ifdef B600
	case 600:     brate = B600; break;
#endif
#ifdef B1200
	case 1200:    brate = B1200; break;
#endif
#ifdef B1800
	case 1800:    brate = B1800; break;
#endif
#ifdef B2400
	case 2400:    brate = B2400; break;
#endif
#ifdef B4800
	case 4800:    brate = B4800; break;
#endif
#ifdef B9600
	case 9600:    brate = B9600; break;
#endif
#ifdef B19200
	case 19200:   brate = B19200; break;
#endif
#ifdef B38400
	case 38400:   brate = B38400; break;
#endif
#ifdef B57600
	case 57600:   brate = B57600; break;
#endif
#ifdef B115200
	case 115200:  brate = B115200; break;
#endif
#ifdef B230400
	case 230400:  brate = B230400; break;
#endif
#ifdef B460800
	case 460800:  brate = B460800; break;
#endif
#ifdef B500000
	case 500000:  brate = B500000; break;
#endif
#ifdef B576000
	case 576000:  brate = B576000; break;
#endif
#ifdef B921600
	case 921600:  brate = B921600; break;
#endif
#ifdef B1000000
	case 1000000: brate = B1000000; break;
#endif
#ifdef B1152000
	case 1152000: brate = B1152000; break;
#endif
#ifdef B1500000
	case 1500000: brate = B1500000; break;
#endif
#ifdef B2000000
	case 2000000: brate = B2000000; break;
#endif
#ifdef B2500000
	case 2500000: brate = B2500000; break;
#endif
#ifdef B3000000
	case 3000000: brate = B3000000; break;
#endif
#ifdef B3500000
	case 3500000: brate = B3500000; break;
#endif
#ifdef B4000000
	case 4000000: brate = B4000000; break;
#endif
	default: return CSP_ERR_DRIVER;
	}

	tcgetattr(csp_usart_fd, &options);
	cfsetispeed(&options, brate);
	cfsetospeed(&options, brate);
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	options.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
	options.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
	options.c_cc[VTIME] = 0;
	options.c_cc[VMIN] = 1;
	tcsetattr(csp_usart_fd, TCSANOW, &options);
	if (tcgetattr(csp_usart_fd, &options) == -1) {
		perror("error setting USART options");
		return CSP_ERR_DRIVER;
	}
	fcntl(csp_usart_fd, F_SETFL, 0);

	/* Flush old transmissions */
	tcflush(csp_usart_fd, TCIOFLUSH);

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
