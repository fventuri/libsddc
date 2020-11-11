/*
 * usb_device.h - Basic USB and USB control functions
 *
 * Copyright (C) 2020 by Franco Venturi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

#include <libusb.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct usb_device usb_device_t;

struct usb_device_info {
  unsigned char *manufacturer;
  unsigned char *product;
  unsigned char *serial_number;
};

enum USBCommands {
  STARTFX3 = 0xaa,
  STOPFX3 = 0xab,
  TESTFX3 = 0xac,
  GPIOFX3 = 0xad,
  I2CWFX3 = 0xae,
  I2CRFX3 = 0xaf,
  DAT31FX3 = 0xb0,
  RESETFX3 = 0xb1,
  SI5351A = 0xb2,
  SI5351ATUNE = 0xb3, /* ??? */
  R820T2INIT = 0xb4,
  R820T2TUNE = 0xb5,
  R820T2SETATT = 0xb6,
  R820T2GETATT = 0xb7,
  R820T2STDBY = 0xb8
};

int usb_device_count_devices();

int usb_device_get_device_list(struct usb_device_info **usb_device_infos);

int usb_device_free_device_list(struct usb_device_info *usb_device_infos);

usb_device_t *usb_device_open(int index, const char* imagefile,
                              uint16_t gpio_register);

int usb_device_handle_events(usb_device_t *this);

void usb_device_close(usb_device_t *this);

int usb_device_control(usb_device_t *this, uint8_t request, uint16_t value,
                       uint16_t index, uint8_t *data, uint16_t length);

uint16_t usb_device_gpio_get(usb_device_t *this);

int usb_device_gpio_set(usb_device_t *this, uint16_t bit_pattern,
                        uint16_t bit_mask);

int usb_device_gpio_on(usb_device_t *this, uint16_t bit_pattern);

int usb_device_gpio_off(usb_device_t *this, uint16_t bit_pattern);

int usb_device_gpio_toggle(usb_device_t *this, uint16_t bit_pattern);

int usb_device_i2c_write(usb_device_t *this, uint8_t i2c_address,
                         uint8_t register_address, uint8_t *data,
                         uint8_t length);

int usb_device_i2c_write_byte(usb_device_t *this, uint8_t i2c_address,
                              uint8_t register_address, uint8_t value);

int usb_device_i2c_read(usb_device_t *this, uint8_t i2c_address,
                        uint8_t register_address, uint8_t *data,
                        uint8_t length);

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE_H */
