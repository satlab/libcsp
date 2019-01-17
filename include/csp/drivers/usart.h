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

#ifndef CSP_USART_H_
#define CSP_USART_H_

#include <stdint.h>

/**
 * USART configuration, to be used with the csp_usart_init call.
 */
struct csp_usart_conf {
	const char *device;
	uint32_t baudrate;
};

/**
 * Initialise UART with the usart_conf data structure
 * @param usart_conf full configuration structure
 */
int csp_usart_init(struct csp_usart_conf *conf);

/**
 * In order to catch incoming chars use the callback.
 * @param callback function pointer
 */
typedef void (*csp_usart_callback_t)(uint8_t *buf, int len, void *pxTaskWoken);
void csp_usart_set_callback(csp_usart_callback_t callback);

/**
 * Polling putchar
 *
 * @param c Character to transmit
 */
void csp_usart_putc(char c);

#endif /* CSP_USART_H_ */
