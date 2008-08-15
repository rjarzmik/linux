/* Driver for the Maxim 1586B PMIC, connected to PXA27x PWR_I2C.
 *
 * Copyright (c) 2006  Michal Sroczynski <msroczyn-ZGDBLhmm9VR4rM3dGMyr8Q@xx>
 * Copyright (c) 2006  Anton Vorontsov <cbou-JGs/UdohzUI@xxxxxxxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * History:
 * 2006-02-04 : Michal Sroczynski <msroczyn-ZGDBLhmm9VR4rM3dGMyr8Q@xxx>
 *	initial driver for Asus730
 * 2006-06-05 : Anton Vorontsov <cbou-JGs/UdohzUI@xxxxxxxxxxxxxxxx>
 *	hx4700 port, various changes
 * 2006-12-06 : Anton Vorontsov <cbou-JGs/UdohzUI@xxxxxxxxxxxxxxxx>
 *	Convert to the generic PXA driver.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <mach/pxa27x_voltage.h>

static inline u_int8_t mv2cmd(int mv)
{
	u_int8_t val = (mv - 700) / 25;
	if (val > 31)
		val = 31;
	return val;
}

static struct pxa27x_voltage_chip chip = {
	.address = 0x14,
	.mv2cmd = mv2cmd
};

static struct platform_device pdev = {
	.name	= "pxa27x-voltage",
	.dev = {
		.platform_data = &chip,
	}
};

static int __init max1586b_init(void)
{
	int ret;

	ret = platform_device_register(&pdev);
	return ret;
}

static void __exit max1586b_exit(void)
{
	platform_device_unregister(&pdev);
	return;
}

module_init(max1586b_init);
module_exit(max1586b_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Sroczynski <msroczyn-ZGDBLhmm9VR4rM3dGMyr8Q@xxxxx>, "
	      "Anton Vorontsov <cbou-JGs/UdohzUI@xxxxxxxxxxxxxxxx>");
MODULE_DESCRIPTION("Driver for the Maxim 1586B PMIC");
