/* Phone interface driver for GPS SIRF-III
 *
 * 2007-12-12 Robert Jarzmik
 *
 * This code is licenced under the GPLv2.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/serial.h>

#include "mioa701.h"

extern struct platform_pxa_serial_funcs mioa701_gps_funcs;

static void mioa701_gps_configure(int state)
{
	//printk( KERN_NOTICE "mioa701 configure gps: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
                try_module_get(THIS_MODULE);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		break;

	case PXA_UART_CFG_POST_SHUTDOWN:
                module_put(THIS_MODULE);

	default:
		break;
	}
}


static int
mioa701_gps_probe(struct platform_device *dev)
{
	/* configure gps UART */
	pxa_gpio_mode(MIO_GPIO_GPS_RXD_MD);
	pxa_gpio_mode(MIO_GPIO_GPS_TXD_MD);

	return 0;
}

static int
mioa701_gps_remove(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver gps_driver = {
	.driver   = {
		.name     = "mioa701-gps",
	},
	.probe    = mioa701_gps_probe,
	.remove   = mioa701_gps_remove,
};

static int __init mioa701_gps_init(void)
{
	printk(KERN_NOTICE "mioa701 Gps Driver\n");
	mioa701_gps_funcs.configure = mioa701_gps_configure;
	pxa_set_stuart_info(&mioa701_gps_funcs);
	return platform_driver_register( &gps_driver );
}

static void __exit mioa701_gps_exit(void)
{
	pxa_set_stuart_info(NULL);
	mioa701_gps_funcs.configure = NULL;
	platform_driver_unregister( &gps_driver );
}

module_init(mioa701_gps_init);
module_exit(mioa701_gps_exit);

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 GPS Support Driver");
MODULE_LICENSE("GPL");
