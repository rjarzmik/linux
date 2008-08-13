/*
 * A simple GPIO VBUS sensing driver for B peripheral only devices
 * with internal transceivers.
 * Optionally D+ pullup can be controlled by a second GPIO.
 *
 * Copyright (c) 2008 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/**
 * struct gpio_vbus_mach_info - configuration for gpio_vbus
 * @gpio_vbus: VBUS sensing GPIO
 * @gpio_vbus_inverted: gpio_vbus is active low
 * @gpio_pullup: optional D+ pullup GPIO
 * @gpio_pullup_inverted: gpio_pullup is active low
 */
struct gpio_vbus_mach_info {
	int gpio_vbus;
	int gpio_vbus_inverted;
	int gpio_pullup;
	int gpio_pullup_inverted;
};
