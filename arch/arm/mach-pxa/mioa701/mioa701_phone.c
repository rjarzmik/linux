/* Phone interface driver for Sagem XS200 on MIO A701
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

static void
mioa701_phone_configure( int state )
{
	int tries;

	//printk( KERN_NOTICE "mioa701 configure phone: %d\n", state );
	switch (state) {
	
	case PXA_UART_CFG_PRE_STARTUP:
		break;

	case PXA_UART_CFG_POST_STARTUP:
		/* pre-serial-up hardware configuration */
		gpio_set_value(GPIO_NR_MIOA701_GSM_UNKNOWN2, 1);
		gpio_set_value(GPIO_NR_MIOA701_GSM_UNKNOWN1, 1);
		gpio_set_value(GPIO_NR_MIOA701_GSM_PREP, 1);
                gpio_set_value(GPIO_NR_MIOA701_GSM_OFF, 0);
		mdelay(100);
		gpio_set_value(GPIO_NR_MIOA701_GSM_PREP, 0);
                mdelay(10);
		gpio_set_value(GPIO_NR_MIOA701_GSM_PREP, 1);

		tries = 0;
		do {
			mdelay(10);
		} while ( (FFMSR & MSR_CTS) && (tries++ < 300) );
		if (tries >= 300) {
			printk("MioA701 phone: expect garbage at GSM start.\n");
		} else {
			printk("MioA701 phone: GSM turned on.\n");
		}
                try_module_get(THIS_MODULE);
		break;

	case PXA_UART_CFG_PRE_SHUTDOWN:
		gpio_set_value(GPIO_NR_MIOA701_GSM_UNKNOWN1, 1);
		mdelay(1);
		gpio_set_value(GPIO_NR_MIOA701_GSM_PREP, 1);
		mdelay(1);
		gpio_set_value(GPIO_NR_MIOA701_GSM_OFF, 1);
		gpio_set_value(GPIO_NR_MIOA701_GSM_UNKNOWN1, 0);
		mdelay(1);
		gpio_set_value(GPIO_NR_MIOA701_GSM_UNKNOWN1, 1);
		gpio_set_value(GPIO_NR_MIOA701_GSM_UNKNOWN2, 0);
		mdelay(1);
		gpio_set_value(GPIO_NR_MIOA701_GSM_UNKNOWN2, 1);
		printk("MioA701 phone: GSM turned off.\n");
		break;

	case PXA_UART_CFG_POST_SHUTDOWN:
                module_put(THIS_MODULE);

	default:
		break;
	}
}


static int
mioa701_phone_probe( struct platform_device *dev )
{
	struct mioa701_phone_funcs *funcs = dev->dev.platform_data;

	/* configure phone GPIOS */
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_OFF | GPIO_OUT | GPIO_DFLT_LOW);
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_PREP | GPIO_OUT);
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UNKNOWN1 | GPIO_OUT );
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UNKNOWN2 | GPIO_OUT );

	/* configure phone UART */
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UART_CTS );
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UART_RTS );
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UART_DTR );
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UART_DSR );
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UART_DCD );
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UART_RXD );
	pxa_gpio_mode( GPIO_NR_MIOA701_GSM_UART_TXD );

	funcs->configure = mioa701_phone_configure;

	return 0;
}

static int
mioa701_phone_remove( struct platform_device *dev )
{
	struct mioa701_phone_funcs *funcs = dev->dev.platform_data;

	funcs->configure = NULL;

	return 0;
}

static struct platform_driver phone_driver = {
	.driver	  = {
		.name	  = "mioa701-phone",
	},
	.probe	  = mioa701_phone_probe,
	.remove	  = mioa701_phone_remove,
};

static int __init
mioa701_phone_init( void )
{
	printk(KERN_NOTICE "mioa701 Phone Driver\n");
	return platform_driver_register( &phone_driver );
}

static void __exit
mioa701_phone_exit( void )
{
	platform_driver_unregister( &phone_driver );
}

module_init( mioa701_phone_init );
module_exit( mioa701_phone_exit );

MODULE_AUTHOR("Robert Jarzmik");
MODULE_DESCRIPTION("MIO A701 Phone Support Driver");
MODULE_LICENSE("GPL");
