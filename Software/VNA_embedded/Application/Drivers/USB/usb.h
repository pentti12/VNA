/*
 * usb.h
 *
 *  Created on: Aug 12, 2020
 *      Author: jan
 */

#ifndef DRIVERS_USB_USB_H_
#define DRIVERS_USB_USB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef void(*usbd_callback_t)(const uint8_t *buf, uint16_t len);

void usb_init(usbd_callback_t callback);
bool usb_transmit(const uint8_t *data, uint16_t length);
void usb_log(const char *log, uint16_t length);


#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_USB_USB_H_ */
