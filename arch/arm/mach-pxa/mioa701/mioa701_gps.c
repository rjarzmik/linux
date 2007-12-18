
/* Phone interface driver for GPS SIRF-III
 *
 *
 * 2007-12-12 Robert Jarzmik
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/gpio.h>
#include <asm/hardware.h>
#include <asm/arch/serial.h>

#include "mioa701.h"

static void
mioa701_gps_configure( int state )
{
	int tries, ready;

	//printk( KERN_NOTICE "mioa701 configure gps: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		break;

	default:
		break;
	}
}


static int
mioa701_gps_probe( struct platform_device *dev )
{
	struct mioa701_gps_funcs *funcs = dev->dev.platform_data;

	/* configure gps UART */
	pxa_gpio_mode( GPIO_NR_MIOA701_GPS_UART_RXD );
	pxa_gpio_mode( GPIO_NR_MIOA701_GPS_UART_TXD );

	funcs->configure = mioa701_gps_configure;
	return 0;
}

static int
mioa701_gps_remove( struct platform_device *dev )
{
	struct mioa701_gps_funcs *funcs = dev->dev.platform_data;

	funcs->configure = NULL;
	return 0;
}

static struct platform_driver gps_driver = {
	.driver   = {
		.name     = "mioa701-gps",
	},
	.probe    = mioa701_gps_probe,
	.remove   = mioa701_gps_remove,
};

static int __init
mioa701_gps_init( void )
{
	printk(KERN_NOTICE "mioa701 Gps Driver\n");
	return platform_driver_register( &gps_driver );
}

static void __exit
mioa701_gps_exit( void )
{
	platform_driver_unregister( &gps_driver );
}

module_init( mioa701_gps_init );
module_exit( mioa701_gps_exit );

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 GPS Support Driver");
MODULE_LICENSE("GPL");
